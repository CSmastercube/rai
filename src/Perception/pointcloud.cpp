#ifdef PCL
#include "pointcloud.h"
#include "perception.h"

#include <numeric>
#include <limits>

#include <Core/module.h>
#include <Core/array.h>

#include <pcl/point_cloud.h>
#include <pcl/features/normal_3d.h>
#include <pcl/filters/passthrough.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/sample_consensus/ransac.h>
#include <pcl/sample_consensus/sac_model_plane.h>
#include <pcl/search/kdtree.h>
#include <pcl/segmentation/extract_clusters.h>
#include <pcl/segmentation/sac_segmentation.h>
#include <pcl/common/transforms.h>

#include <vtkSmartPointer.h>
#include <vtkDataSet.h>
#include <vtkLineSource.h>
#include <vtkTubeFilter.h>


ObjectClusterer::ObjectClusterer() : Process("ObjectClusterer") {
  biros().getVariable(data_3d, "KinectData3D", this, true);
  biros().getVariable(point_clouds, "ObjectClusters", this, true);
  listenTo(data_3d);
}

void findMinMaxOfCylinder(double &min, double &max, arr &start, const pcl::PointCloud<PointT>::Ptr &cloud, const arr &direction) {
  arr dir = direction/length(direction);
  min = std::numeric_limits<double>::max();
  max = -std::numeric_limits<double>::max();
  for(uint i=0; i<cloud->size(); ++i) {
    arr point = ARR((*cloud)[i].x, (*cloud)[i].y, (*cloud)[i].z);
    double p = scalarProduct(dir, point);
    if(p < min) {
      min = p;
      copy(start, point);
    }
    if(p > max) max = p;
  }
}

struct sObjectFitter{
  sObjectFitter(ObjectFitter* p) : p(p) {}
  ObjectFitter *p;

  FittingJob createNewJob(const pcl::PointCloud<PointT>::Ptr &cloud, const pcl::PointIndices::Ptr &inliers){ 
    FittingJob outliers(new pcl::PointCloud<PointT>());
    pcl::ExtractIndices<PointT> extract;
    extract.setInputCloud(cloud);
    extract.setIndices(inliers);
    extract.setNegative(true);
    extract.filter(*outliers);
    return outliers;
  }
  
  double confidenceForCylinder(pcl::PointIndices::Ptr &inliers, FittingResult& object, const pcl::PointCloud<PointT>::Ptr &cloud, const pcl::PointCloud<pcl::Normal>::Ptr &normals) {
    // Create the segmentation object for cylinder segmentation and set all the parameters
    // TODO: make parameters
    pcl::SACSegmentationFromNormals<PointT, pcl::Normal> seg; 
    seg.setOptimizeCoefficients (true);
    seg.setModelType (pcl::SACMODEL_CYLINDER);
    seg.setMethodType (pcl::SAC_RANSAC);
    double ndw = biros().getParameter<double>("CylNormalDistanceWeight", 0.07, p);
    seg.setNormalDistanceWeight (ndw);
    seg.setMaxIterations (100);
    double dt = biros().getParameter<double>("CylDistanceThreshold", 0.01, p);
    seg.setDistanceThreshold (dt);
    double minRadius = biros().getParameter<double>("MinSphereRadius", 0.01, p);
    double maxRadius = biros().getParameter<double>("MaxSphereRadius", 0.1, p);
    seg.setRadiusLimits (minRadius, maxRadius);
    seg.setInputCloud (cloud);
    seg.setInputNormals (normals);

    pcl::PointIndices::Ptr inliers_cylinder (new pcl::PointIndices);
    pcl::ModelCoefficients::Ptr coefficients_cylinder (new pcl::ModelCoefficients);
    seg.segment (*inliers_cylinder, *coefficients_cylinder);

    uint minCloudSize = biros().getParameter<int>("minCloudSize", 500, p);
    if (inliers_cylinder->indices.size() < minCloudSize) {
      object.reset();   
      return 0;
    }
    else {
      object = coefficients_cylinder;
      inliers = inliers_cylinder;
      return 1./(1 + cloud->size() - inliers->indices.size());
    }
  }
  
  double confidenceForSphere(pcl::PointIndices::Ptr &inliers, FittingResult& object, const pcl::PointCloud<PointT>::Ptr &cloud, const pcl::PointCloud<pcl::Normal>::Ptr &normals) {
    // Create the segmentation object for sphere segmentation and set all the parameters
    // TODO: make parameters
    pcl::SACSegmentationFromNormals<PointT, pcl::Normal> seg; 
    seg.setOptimizeCoefficients (true);
    seg.setModelType (pcl::SACMODEL_SPHERE);
    seg.setMethodType (pcl::SAC_RANSAC);
    double ndw = biros().getParameter<double>("SphereNormalDistanceWeight", 10, p);
    seg.setNormalDistanceWeight (ndw);
    seg.setMaxIterations (100);
    double dt = biros().getParameter<double>("SphereDistanceThreshold", .0005, p);
    seg.setDistanceThreshold (dt);
    seg.setRadiusLimits (0.01, 0.1);
    seg.setInputCloud (cloud);
    seg.setInputNormals (normals);

    pcl::PointIndices::Ptr inliers_sphere(new pcl::PointIndices);
    pcl::ModelCoefficients::Ptr coefficients_sphere(new pcl::ModelCoefficients);
    seg.segment (*inliers_sphere, *coefficients_sphere);
    uint minCloudSize = biros().getParameter<int>("minCloudSize", 500, p);
    if (inliers_sphere->indices.size() < minCloudSize) {
      object.reset();   
      return 0;
    }
    else {
      object = coefficients_sphere;
      inliers = inliers_sphere;
     
      // if rest points are enough create new job
      //if (cloud->size() - inliers_sphere->indices.size() > 500) {
        //createNewJob(cloud, inliers_sphere);
      //}
      return 1./(1 + cloud->size() - inliers->indices.size());
    }
  }

  bool doWork(FittingResult &object, FittingJob &anotherJob, const FittingJob &cloud) {
    // Build kd-tree from cloud
    pcl::search::KdTree<PointT>::Ptr tree (new pcl::search::KdTree<PointT>);
    tree->setInputCloud (cloud);

    // Estimate point normals
    pcl::NormalEstimation<PointT, pcl::Normal> ne;
    ne.setSearchMethod (tree);
    ne.setInputCloud (cloud);
    ne.setKSearch (50);

    pcl::PointCloud<pcl::Normal>::Ptr cloud_normals (new pcl::PointCloud<pcl::Normal>);
    ne.compute (*cloud_normals);

    FittingResult cyl_object;
    pcl::PointIndices::Ptr cyl_inliers;
    double cylinder_confidence = confidenceForCylinder(cyl_inliers, cyl_object, cloud, cloud_normals);
    FittingResult sphere_object;
    pcl::PointIndices::Ptr sphere_inliers;
    double sphere_confidence = confidenceForSphere(sphere_inliers, sphere_object, cloud, cloud_normals);

    double threshold = 0.;

    pcl::PointIndices::Ptr inliers;
    if (sphere_confidence > threshold && sphere_confidence > cylinder_confidence) {
      object = sphere_object; 
      inliers = sphere_inliers;
    }
    else if (cylinder_confidence > threshold) {
      object = cyl_object;  
      inliers = cyl_inliers;
      double min, max;
      arr direction = ARR(object->values[3], object->values[4], object->values[5]);
      pcl::PointCloud<PointT>::Ptr cylinder(new pcl::PointCloud<PointT>());
      pcl::ExtractIndices<PointT> extract;
      extract.setInputCloud(cloud);
      extract.setIndices(inliers);
      extract.setNegative(false);
      extract.filter(*cylinder);
      arr start;
      findMinMaxOfCylinder(min, max, start, cylinder, direction);
      arr s = ARR(object->values[0], object->values[1], object->values[2]);
      direction = direction/length(direction);
      arr st = s+scalarProduct(direction, (start-s))*direction;
      direction = (max - min) * direction;
      object->values[0] = st(0);
      object->values[1] = st(1);
      object->values[2] = st(2);
      object->values[3] = direction(0);
      object->values[4] = direction(1);
      object->values[5] = direction(2);

      //DEBUG_VAR(pointcloud, st);
    }
    else {
      object.reset();  
      return false;
    }
    //if rest points are enough create new job

    uint minCloudSize = biros().getParameter<int>("minCloudSize", 500, p);
    if (cloud->size() - inliers->indices.size() > minCloudSize) {
      anotherJob = createNewJob(cloud, inliers);
      return true;
    }
    return false;
  }
};

void ObjectClusterer::open() {}

void ObjectClusterer::close() {}

void ObjectClusterer::step() {
  //get a copy of the kinect data
  pcl::PointCloud<PointT>::Ptr cloud(data_3d->get_point_cloud_copy(this));
  if(cloud->points.size() == 0) return;

  //DEBUG_VAR(pointcloud, cloud->points.size());

  // filter all points too far away
  pcl::PointCloud<PointT>::Ptr cloud_filtered(new pcl::PointCloud<PointT>());
  pcl::PassThrough<PointT> passthrough;
  passthrough.setInputCloud(cloud);
  passthrough.setFilterFieldName("z");
  passthrough.setFilterLimits(0,1.0);
  passthrough.filter(*cloud_filtered);
  passthrough.setInputCloud(cloud_filtered);
  passthrough.setFilterFieldName("x");
  passthrough.setFilterLimits(-0.4,0.25);
  passthrough.filter(*cloud_filtered);
 
  // filter away the table. This is done by fitting a plane to all data and
  // remove all inliers. This assumes that there is one big plane, which
  // contains all other objects.
  pcl::IndicesPtr inliers(new std::vector<int>);
  pcl::SampleConsensusModelPlane<PointT>::Ptr planemodel 
    (new pcl::SampleConsensusModelPlane<PointT> (cloud_filtered));
  pcl::RandomSampleConsensus<PointT> ransac(planemodel);
  ransac.setDistanceThreshold(0.01);
  ransac.computeModel();
  ransac.getInliers(*inliers);

  pcl::ExtractIndices<PointT> extract;
  extract.setInputCloud(cloud_filtered);
  extract.setIndices(inliers);
  extract.setNegative(true);
  extract.filter(*cloud_filtered);

  if (cloud_filtered->points.size() == 0) return;

  pcl::search::KdTree<PointT>::Ptr tree (new pcl::search::KdTree<PointT>);
  tree->setInputCloud (cloud_filtered);

  std::vector<pcl::PointIndices> cluster_indices;
  pcl::EuclideanClusterExtraction<PointT> ec;
  ec.setClusterTolerance(0.01);
  int minCloudSize = biros().getParameter<int>("minCloudSize", 500, this);
  ec.setMinClusterSize(minCloudSize);
  ec.setMaxClusterSize(25000);
  ec.setSearchMethod(tree);
  ec.setInputCloud(cloud_filtered);
  ec.extract(cluster_indices);

  PointCloudL _point_clouds;
  // append cluster to PointCloud list 
  for (std::vector<pcl::PointIndices>::const_iterator it = cluster_indices.begin (); it != cluster_indices.end (); ++it) {
    pcl::PointCloud<PointT>::Ptr cloud_cluster (new pcl::PointCloud<PointT>);
    for (std::vector<int>::const_iterator pit = it->indices.begin (); pit != it->indices.end (); pit++)
      cloud_cluster->points.push_back (cloud_filtered->points[*pit]); 
    cloud_cluster->width = cloud_cluster->points.size ();
    cloud_cluster->height = 1;
    cloud_cluster->is_dense = true;
    pcl::PointCloud<PointT>::Ptr cluster_transformed(new pcl::PointCloud<PointT>);
    Eigen::Matrix4f transform(biros().getParameter<floatA>("kinect_trans_mat", this).p);
    transform.transposeInPlace();
    pcl::transformPointCloud(*cloud_cluster, *cluster_transformed, transform);

    _point_clouds.append(cluster_transformed);
  }

  point_clouds->set_point_clouds(_point_clouds, this);
}

ObjectFitter::ObjectFitter() : Process("ObectFitter"), s(new sObjectFitter(this)) {
  biros().getVariable<PointCloudSet>(objectClusters, "ObjectClusters", this, true);
  biros().getVariable<ObjectSet>(objects, "Objects", this, true);
  listenTo(objectClusters);
}

void ObjectFitter::open() {}

void ObjectFitter::step() {
  MT::Array<FittingResult> results;

  PointCloudL plist = objectClusters->get_point_clouds(this);
  PointCloudL next;
  while (plist.N) {
#pragma omp parallel for
    for(uint i=0; i<plist.N; ++i) {
      FittingResult result;
      FittingJob anotherJob;
      bool put = s->doWork(result, anotherJob, plist(i));
#pragma omp critical
      {
        if(put) next.append(anotherJob);
        results.append(result);
      }
    } 
    plist = next;
    next.clear();
  }

  //write back
  objects->writeAccess(this);
  objects->objects = results;
  objects->deAccess(this);
}
void ObjectFitter::close() {}

struct sObjectFilter {
  bool filterShape(arr& pos, intA& nums, const arr& measurement, const int i, const double epsilon ) {
    if (length(measurement - pos[i]) < epsilon)  {
      pos[i] = pos[i] * ((nums(i)-1.)/nums(i)) + measurement * (1./nums(i));
      nums(i)++;
      return true;
    }
    return false;
  }

  void filterCylinders(arr& pos, FittingResultL& cylinders, const FittingResultL& objects, Process *p) {
    intA nums;
    pos.clear();
    nums.resize(0);
    pos.resize(0,7);
    for (uint i = 0; i< objects.N; ++i) {
      if (!objects(i)) continue; 
      if (objects(i)->values.size() != 7) continue;
      cylinders.append(objects(i));
      bool found = false;
      arr measurement;
      measurement.resize(1,7);
      std::copy(objects(i)->values.begin(), objects(i)->values.end(), measurement.p);
      arr measurement_;
      //DEBUG_VAR(pointcloud, measurement);
      arr pos_ = measurement.sub(0,0,0,2)+0.5*measurement.sub(0,0,3,5);
      measurement(0,0) = pos_(0,0);
      measurement(0,1) = pos_(0,1);
      measurement(0,2) = pos_(0,2);
      //DEBUG_VAR(pointcloud, measurement);
      measurement_.append(measurement.sub(0,0,0,2));
      measurement_.append(-measurement.sub(0,0,3,5));
      measurement_.append(measurement(0,6));
      measurement_.resize(1,7);
      double epsilon = biros().getParameter<double>("objectDistance", p);
      for (uint j = 0; j<pos.d0; ++j) {
        if(filterShape(pos, nums, measurement, j, epsilon)) { found = true; break;}
        else if (filterShape(pos, nums, measurement_, j, epsilon)) {found = true; break; }
      }
      if(!found) {
        pos.append(measurement);
        nums.append(1);
      }
    }

  }
  void filterSpheres(arr& pos, FittingResultL& spheres, const FittingResultL& objects, Process* p) {
    intA nums;
    pos.clear();
    nums.resize(0);
    pos.resize(0,4);
    for (uint i = 0; i< objects.N; ++i) {
      if (!objects(i)) continue;
      if (objects(i)->values.size() != 4) continue;
      spheres.append(objects(i));
      bool found = false;
      arr measurement;
      measurement.resize(1,4);
      std::copy(objects(i)->values.begin(), objects(i)->values.end(), measurement.p);
      double epsilon = biros().getParameter<double>("objectDistance", p);
      for (uint j = 0; j<pos.d0; ++j) {
        if(filterShape(pos, nums, measurement, j, epsilon)) { found = true; break; }
      }
      if(!found) {
        pos.append(measurement);
        nums.append(1);
      }
    }
  }
};

ObjectFilter::ObjectFilter(const char* name) : Process(name) {
  s = new sObjectFilter;  
  biros().getVariable(in_objects, "Objects", this, true);
  biros().getVariable(out_objects, "filteredObjects", this, true);
  listenTo(in_objects);
}

void ObjectFilter::open() {
}

void ObjectFilter::step() {
  arr cyl_pos, sph_pos;
  cyl_pos.resize(0,7);
  sph_pos.resize(0,7);
  FittingResultL pcl_cyls, pcl_sph;
  in_objects->readAccess(this);
  FittingResult o;
  s->filterCylinders( cyl_pos, pcl_cyls, in_objects->objects, this);
  s->filterSpheres( sph_pos, pcl_sph, in_objects->objects, this);
  in_objects->deAccess(this);
  out_objects->writeAccess(this);
  out_objects->objects.clear();
  // HACK! We assume max two cylinders
  for (uint i = 0; i<cyl_pos.d0; i++) {
    double height = length(ARR(cyl_pos(i,3), cyl_pos(i,4), cyl_pos(i,5)));
    // if cylinder is higher then real cylinder there are probably two...
    if (height > 0.15) {
      int oldd0 = cyl_pos.d0;
      cyl_pos(i, 3) *= .5;
      cyl_pos(i, 4) *= .5;
      cyl_pos(i, 5) *= .5;
      cyl_pos(i, 0) += .5*cyl_pos(i, 3);
      cyl_pos(i, 1) += .5*cyl_pos(i, 4);
      cyl_pos(i, 2) += .5*cyl_pos(i, 5);
      cyl_pos.append(cyl_pos.sub(i,i,0,-1));
      cyl_pos.reshape(oldd0+1, 7);
      cyl_pos(oldd0, 0) -= cyl_pos(i, 3);
      cyl_pos(oldd0, 1) -= cyl_pos(i, 4);
      cyl_pos(oldd0, 2) -= cyl_pos(i, 5);
    }
  }
  for (uint i = 0; i<cyl_pos.d0; i++) {
    ObjectBelief *cyl = new ObjectBelief();
    cyl->position = ARR(cyl_pos(i,0), cyl_pos(i,1), cyl_pos(i,2));
    cyl->rotation.setDiff(ARR(0,0,1), ARR(cyl_pos(i,3), cyl_pos(i,4), cyl_pos(i,5)));
    cyl->shapeParams(RADIUS) = cyl_pos(i,6);//.025;
    cyl->shapeParams(HEIGHT) = length(ARR(cyl_pos(i,3), cyl_pos(i,4), cyl_pos(i,5)));
    cyl->shapeType = ors::cylinderST;
    //cyl->pcl_object = pcl_cyls(i);
    out_objects->objects.append(cyl);
  }
  for (uint i = 0; i<sph_pos.d0; i++) {
    ObjectBelief *sph = new ObjectBelief;
    sph->position = ARR(sph_pos(i,0), sph_pos(i,1), sph_pos(i,2));
    sph->shapeParams(RADIUS) = sph_pos(i,3);
    sph->shapeType = ors::sphereST;
    //sph->pcl_object = pcl_sph(i);
    out_objects->objects.append(sph);
  }
  out_objects->deAccess(this);
}

ObjectTransformator::ObjectTransformator(const char* name) : Process(name) {
  biros().getVariable(kinect_objects, "filteredObjects", this, true);
  geo.init("GeometricState", this);
  listenTo(kinect_objects);
}

void ObjectTransformator::open() {
}

void createOrsObject(ors::Body& body, const ObjectBelief *object, const arr& transformation) {
  ors::Transformation t;
  t.pos = object->position;
  t.rot = object->rotation;

  ors::Transformation sensor_to_ors;
  sensor_to_ors.setAffineMatrix(transformation.p);

  t.appendTransformation(sensor_to_ors);

  arr size = ARR(0., 0., object->shapeParams(HEIGHT), object->shapeParams(RADIUS));
 
  ors::Shape* s = new ors::Shape();
  for (uint i = 0; i < 4; ++i) s->size[i] = size(i);
  for (uint i = 0; i < 3; ++i) s->color[i] = .3;
  s->type = object->shapeType;
  s->body = &body;
  s->name = "pointcloud_shape";
  
  body.shapes.append(s);
  body.X = t; 
}

void moveObject(intA& used, const ShapeL& objects, const ors::Vector& pos, const ors::Quaternion& rot) {
  double max = std::numeric_limits<double>::max();
  int max_index = -1;
  for(uint i=0; i<objects.N; ++i) {
    if(used.contains(i)) continue;
    ors::Vector diff_ = objects(i)->X.pos - pos;
    double diff = length(ARR(diff_.x, diff_.y, diff_.z));
    if(diff < max) {
      max = diff;
      max_index = i;
    }
  }
  objects(max_index)->X.pos = pos;
  objects(max_index)->X.rot = rot;
  objects(max_index)->rel.setDifference(objects(max_index)->body->X, objects(max_index)->X);
  used.append(max_index);
}

void ObjectTransformator::step() {
  DEBUG(pointcloud, "geo pull");
  geo.pull();
  DEBUG(pointcloud, "done");

  ShapeL cylinders;
  ShapeL spheres;
  for (int i=geo().ors.shapes.N-1;i>=0;i--) {
    if (strncmp(geo().ors.shapes(i)->name, "thing", 5) == 0 && geo().ors.shapes(i)->type == ors::cylinderST) {
      ors::Shape *s = geo().ors.shapes(i);
      cylinders.append(s);
    }
    else if (strncmp(geo().ors.shapes(i)->name, "thing", 5) == 0 && geo().ors.shapes(i)->type == ors::sphereST) {
      ors::Shape *s = geo().ors.shapes(i);
      spheres.append(s);
    }
  }

  DEBUG(pointcloud, "filteredObject mutex");
  kinect_objects->readAccess(this);
  uint c = 0, s = 0;
  intA used;
  used.clear();
  for(uint i=0;i<kinect_objects->objects.N && i<spheres.N + cylinders.N;i++){
    if (kinect_objects->objects(i)->shapeType == ors::cylinderST && c < cylinders.N) {
      moveObject(used, cylinders, kinect_objects->objects(i)->position, kinect_objects->objects(i)->rotation);
      ++c;
    }
    else if (kinect_objects->objects(i)->shapeType == ors::sphereST && s < spheres.N) {
      spheres(s)->X.pos = kinect_objects->objects(i)->position;
      spheres(s)->X.rot = kinect_objects->objects(i)->rotation;
      spheres(s)->rel.setDifference(spheres(s)->body->X, spheres(s)->X);
      ++s;
    }
  }
  kinect_objects->deAccess(this);
  DEBUG(pointcloud, "filtered Objects mutex unlock");

  geo().ors.calc_fwdPropagateFrames();
  DEBUG(pointcloud, "set ors");
  geo.var->set_ors(geo().ors, this);
  DEBUG(pointcloud, "done.");
}

PointCloudVar::PointCloudVar(const char *name) : Variable(name), point_cloud(new pcl::PointCloud<PointT>()) { 
  reg_point_cloud();
}
#endif // PCL
