#include <stdlib.h>
#include <GL/gl.h>

#include <Geo/signedDistanceFunctions.h>
#include <Geo/mesh.h>
#include <Gui/opengl.h>

#include <Optim/newton.h>

//===========================================================================

void TEST(DistanceFunctions) {
  rai::Transformation t;
  t.setRandom();
  rai::Mesh m;
  m.C = {.5, .5, .5, .5};

  struct DrawPair : GLDrawer {
    arr P1, P2;

    void glDraw(OpenGL& gl){
      glColor(0., 1., 0., 1.);
      glDrawDiamond(P1(0), P1(1), P1(2), .05, .05, .05);

      glColor(0., 0., 1., 1.);
      glDrawDiamond(P2(0), P2(1), P2(2), .05, .05, .05);

      glColor(1., 0., 0., 1.);
      glLineWidth(2.f);
      glDrawProxy(P1, P2, .02);
      glLineWidth(1.f);
      glLoadIdentity();
    }
  } pairDrawer;

  OpenGL gl;
  gl.add(glStandardScene,nullptr);
  gl.add(pairDrawer);
  gl.add(m);

  rai::Array<shared_ptr<SDF>> fcts = {
    make_shared<SDF_SuperQuadric>(t, arr{2., 2., 2.}, 1.2),
//    make_shared<SDF_Sphere>(t, 1.),
//    make_shared<SDF_ssBox>(t, 1., 2., 3., 1.),
//    make_shared<SDF_Cylinder>(t, 2., 1.),
//    make_shared<SDF_Capsule>(t, 2., 1.)
  };

  auto f2 = make_shared<SDF_GridData>(*fcts(0), arr{-2.,-2.,-2.}, arr{2.,2.,2.}, uintA{100,100,100});
  fcts.append(f2);

  for(shared_ptr<SDF>& f: fcts){
    m.setImplicitSurfaceBySphereProjection(*f, 3.);

    //-- check hessian and gradient
    for(uint i=0;i<10;i++){
      arr x(3);
      rndUniform(x, -1., 1.);
      bool suc=true;
      suc &= checkGradient(*f, x, 1e-6);
      suc &= checkHessian(*f, x, 1e-6);

      {
        arr g;
        double d = (*f)(g, NoArr, x);
        pairDrawer.P1 = x;
        pairDrawer.P2 = x-d*g;
        gl.update(0, true);
      }

      if(!suc){
        arr g,H;
        (*f)(g,H,x); //set breakpoint here;
        LOG(-1)  <<"x=" <<x;
      }
    }

    //-- display
    gl.watch();
  }
}

//===========================================================================

void TEST(DistanceFunctions2) {
  //-- check hessian and gradient
  for(uint i=0;i<100;i++){
    arr x(14);
    rndUniform(x, -5., 5.);

    bool suc=true;
    suc &= checkGradient(DistanceFunction_SSBox, x, 1e-6);
//    suc &= checkHessian(SDF_SSBox, x, 1e-6);
    if(!suc){
      arr g,H;
      cout <<"f=" <<DistanceFunction_SSBox(g,H,x); //set breakpoint here;
      HALT("x=" <<x);
    }
  }

  //-- display
//  rai::Mesh m;
//  m.setImplicitSurface(SDF_SSBox,-10.,10.,100); //only works for 3D ScalarFunction
//  OpenGL gl;
//  gl.add(m);
//  gl.watch();

}

//===========================================================================
//
// implicit surfaces
//

void TEST(SimpleImplicitSurfaces) {
  rai::Transformation pose;
  pose.setRandom();

  rai::Array<shared_ptr<SDF>> fcts = {
    make_shared<SDF_ssBox>(pose, arr{1., 2., 3.}, .2),
    make_shared<SDF_Blobby>(),
    make_shared<SDF_Torus>(),
  };

  fcts.append( make_shared<SDF_GridData>(*fcts(0), arr{-2.,-3.,-4.}, arr{2.,3.,4.}, uintA{20,20,20}) );
  fcts.append( make_shared<SDF_GridData>(*fcts(1), arr{-5.,-5.,-5.}, arr{5.,5.,5.}, uintA{100,100,100}) );
  fcts.append( make_shared<SDF_GridData>(*fcts(2), arr{-5.,-5.,-5.}, arr{5.,5.,5.}, uintA{100,100,100}) );

  rai::Mesh m;
  OpenGL gl;
  gl.add(glStandardScene,nullptr);
  gl.add(m);

  for(shared_ptr<SDF>& f: fcts){
    m.setImplicitSurface(*f,-10.,10.,100);
    gl.watch();
  }
}

//===========================================================================

void projectToSurface(){
  rai::Transformation pose;
  pose.setRandom();

  rai::Array<shared_ptr<ScalarFunction>> fcts = {
    make_shared<SDF_Sphere>(pose, 1.),
    make_shared<SDF_ssBox>(pose, arr{1., 2., 3.}, .2),
    make_shared<SDF_Cylinder>(pose, 2., .2),
    make_shared<SDF_Capsule>(pose, 2., .2)
  };

  rai::Mesh m;
  OpenGL gl;
  gl.drawOptions.drawWires=true;
  gl.add(glStandardScene,nullptr);
  gl.add(m);

  ofstream fil("z.obj");

  for(shared_ptr<ScalarFunction>& fct:fcts){
    for(uint k=0;k<1000;k++){
      arr x = randn(3);
      x += pose.pos.getArr();
      checkGradient(*fct, x, 1e-4);
      checkHessian(*fct, x, 1e-4);
      x.writeRaw(fil);
      fil <<(*fct)(NoArr, NoArr, x) <<endl;
    }

    m.setImplicitSurfaceBySphereProjection(*fct, 10., 3);

    gl.watch();
  }
}

//===========================================================================

void display(){
  rai::Transformation pose=0;
//  pose.setRandom();

  rai::Array<shared_ptr<SDF>> fcts = {
    make_shared<SDF_Sphere>(pose, 1.),
//    make_shared<SDF_ssBox>(pose, arr{1., 2., 3.}, .0),
//    make_shared<SDF_Cylinder>(pose, 2., .2),
//    make_shared<SDF_Capsule>(pose, 2., .2)
  };

  auto f2 = make_shared<SDF_GridData>(*fcts(0), arr{-1.,-1.,-1.}, arr{1.,1.,1.}, uintA{20,20,20});
  fcts.append(f2);

  OpenGL gl;

  for(shared_ptr<SDF>& fct:fcts){
    for(double z=-2.; z<=2.; z+= .1) {
      arr samples = grid({-2.,-2., z}, {2.,2., z}, {100, 100, 0});
      arr values = fct->eval(samples);
      values.reshape(101,101);
      cout <<"slice z=" <<z <<endl;
      gl.displayGrey(values, false, 4.);
      rai::wait(.1);
    }
  }
}

//===========================================================================

int MAIN(int argc, char** argv){
  rai::initCmdLine(argc, argv);

  testDistanceFunctions();
  testDistanceFunctions2();
  testSimpleImplicitSurfaces();

  projectToSurface();
  display();

  return 0;
}
