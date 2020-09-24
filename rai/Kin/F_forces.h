/*  ------------------------------------------------------------------
    Copyright (c) 2011-2020 Marc Toussaint
    email: toussaint@tu-berlin.de

    This code is distributed under the MIT License.
    Please see <root-path>/LICENSE for details.
    --------------------------------------------------------------  */

#pragma once

#include "feature.h"

//===========================================================================
// trivial read out of forces

struct F_fex_POA : Feature {
  void phi2(arr& y, arr& J, const FrameL& F);
  uint dim_phi2(const FrameL& F) { return 3; }
};

struct F_fex_Force : Feature {
  void phi2(arr& y, arr& J, const FrameL& F);
  uint dim_phi2(const FrameL& F) { return 3; }
};

struct F_fex_Wrench : Feature {
  void phi2(arr& y, arr& J, const FrameL& F);
  uint dim_phi2(const FrameL& F){ return 6; }
};

//===========================================================================
// totals acting on a joint or object

struct F_HingeXTorque : Feature {
  void phi2(arr& y, arr& J, const FrameL& F);
  uint dim_phi2(const FrameL& F){ return 1; }
};

struct F_ObjectTotalForce : Feature {
  double gravity=9.81;
  bool transOnly=false;
  F_ObjectTotalForce(bool _transOnly=false, bool _zeroGravity=false);
  virtual void phi2(arr& y, arr& J, const FrameL& F);
  virtual uint dim_phi2(const FrameL& C){ if(transOnly) return 3;  return 6; }
};

//===========================================================================
// dynamics

struct F_NewtonEuler : Feature {
  double gravity=9.81;
  F_NewtonEuler(bool _transOnly=false) {
    order = 2;
    gravity = rai::getParameter<double>("TM_NewtonEuler/gravity", 9.81);
  }
  virtual void phi2(arr& y, arr& J, const FrameL& F);
  virtual uint dim_phi2(const FrameL& F) { return 6; }
};

struct F_NewtonEuler_DampedVelocities : Feature {
  double gravity=9.81;
  bool onlyXYPhi=false;
  F_NewtonEuler_DampedVelocities(double _gravity=-1., bool _onlyXYPhi=false) : onlyXYPhi(_onlyXYPhi) {
    order = 1;
    if(_gravity>=0.) {
      gravity = _gravity;
    } else {
      gravity = rai::getParameter<double>("TM_NewtonEuler/gravity", 9.81);
    }
  }
  virtual void phi2(arr& y, arr& J, const FrameL& F);
  virtual uint dim_phi2(const FrameL& F) { return 6; }
};

struct F_Energy : Feature {
  double gravity=9.81;
  F_Energy() {
    order=1;
    gravity = rai::getParameter<double>("TM_Physics/gravity", 9.81);
  }
  virtual void phi2(arr& y, arr& J, const FrameL& F);
  virtual uint dim_phi2(const FrameL& F) {  return 1;  }
};

//===========================================================================
// force geometry, complementarity, velocities

struct F_fex_ForceIsNormal : Feature {
  void phi2(arr& y, arr& J, const FrameL& F);
  uint dim_phi2(const FrameL& F) { return 3; }
};

struct F_fex_ForceIsComplementary : Feature {
  void phi2(arr& y, arr& J, const FrameL& F);
  uint dim_phi2(const FrameL& F);
};

struct F_fex_ForceIsPositive : Feature {
  void phi2(arr& y, arr& J, const FrameL& F);
  uint dim_phi2(const FrameL& F) { return 1; }
};

struct F_fex_POAmovesContinuously : Feature {
  void phi2(arr& y, arr& J, const FrameL& F);
  uint dim_phi2(const FrameL& F) { return 3; }
};

struct F_fex_NormalForceEqualsNormalPOAmotion: Feature {
  void phi2(arr& y, arr& J, const FrameL& F);
  uint dim_phi2(const FrameL& F) { return 1; }
};

struct F_fex_POAzeroRelVel : Feature {
  bool normalOnly=false;
  F_fex_POAzeroRelVel(bool _normalOnly=false) : normalOnly(_normalOnly) { order=1; }
  void phi2(arr& y, arr& J, const FrameL& F);
  uint dim_phi2(const FrameL& F) { if(normalOnly) return 1; return 3; }
};

struct F_fex_ElasticVel : Feature {
  double elasticity, stickiness;
  F_fex_ElasticVel(double _elasticity, double _stickiness) : elasticity(_elasticity), stickiness(_stickiness) { order=1; }
  void phi2(arr& y, arr& J, const FrameL& F);
  uint dim_phi2(const FrameL& F) { return 4; }
};

struct F_fex_NormalVelIsComplementary : Feature {
  double elasticity, stickiness;
  F_fex_NormalVelIsComplementary(double _elasticity, double _stickiness) : elasticity(_elasticity), stickiness(_stickiness) { order=1; }
  void phi2(arr& y, arr& J, const FrameL& F);
  uint dim_phi2(const FrameL& F) { return 1; }
};

struct F_fex_POAisInIntersection_InEq : Feature {
  double margin=0.;
  F_fex_POAisInIntersection_InEq(double _margin=0.) : margin(_margin) {}
  void phi2(arr& y, arr& J, const FrameL& F);
  uint dim_phi2(const FrameL& F) { return 2; }
};

struct F_fex_POA_isAtWitnesspoint : Feature {
  bool use2ndObject=false;
  F_fex_POA_isAtWitnesspoint(bool _use2ndObject=false) : use2ndObject(_use2ndObject) {}
  void phi2(arr& y, arr& J, const FrameL& F);
  uint dim_phi2(const FrameL& F) { return 3; }
};
