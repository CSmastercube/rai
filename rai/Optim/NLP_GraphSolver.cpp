/*  ------------------------------------------------------------------
    Copyright (c) 2011-2020 Marc Toussaint
    email: toussaint@tu-berlin.de

    This code is distributed under the MIT License.
    Please see <root-path>/LICENSE for details.
    --------------------------------------------------------------  */

#include "NLP_GraphSolver.h"
#include "optimization.h"

//===========================================================================

int NLP_GraphSolver::chooseNextVariableToAssign(const uintA& Y) {
  uint n = P->variableDimensions.N;
  for(uint i=n; i--;) {
//  for(uint i=0;i<n;i++){
    if(!Y.contains(i)) return i;
  }
  return -1;
}

uintA NLP_GraphSolver::getVariablesForObjectives(uintA& O) {
  uintA X;
  for(uint k:O) for(int i:P->featureVariables(k)) if(i>=0) X.setAppendInSorted(i);
  return X;
}

void NLP_GraphSolver::evaluate(const arr& x) {
  P->subSelect({}, {});
  P->evaluate(NoArr, NoArr, x);
}

bool NLP_GraphSolver::run() {
  uintA Y; //assigned variables
  uintA X; //active variables
  f_low=0.;

  for(;;) {
    int next = chooseNextVariableToAssign(Y);
    if(next<0) return true; //success: no next variable
    X = {uint(next)};
    cout <<"** GraphSolver: solve (" <<X <<"|" <<Y <<")" <<endl;
    P->subSelect(X, Y);
    std::shared_ptr<SolverReturn> ret = subSolver
                                        .setProblem(P)
                                        .solve();
    while(!ret->feasible) {
      return false;
#if 0
      if(!Y.N) {
        infeasibleSubset = X;
//        conflictSet = G_XY.conflictSet;
        return false;
      }
      /*
      SubGraphProblem G_X(G, X, {});
      G_X.optim();
      f_low = rai::MAX(f_low, G_X.f);
      if(!G_X.feasible){
        infeasibleSubset = X;
        conflictSet = G_X.conflictSet;
        return false;
      }
      */
      uintA pi_C = getVariablesForObjectives(G_XY.conflictSet);
      setMinusSorted(pi_C, X);
      if(!pi_C.N) {
        uint prev = Y.popLast();
        X.setAppendInSorted(prev);
      } else {
        for(uint i:pi_C) {
          Y.removeValue(i, false);
          X.setAppendInSorted(i);
        }
      }
      CHECK(X.isSorted(), "");
      for(uint i:Y) CHECK(!X.containsInSorted((i)), "");
      P->subSelect(X, Y);
      ret = subSolver.solve();
#endif
    }
    Y.append(X);
  }

  return true;
}

void NLP_GraphSolver::test() {
  P->subSelect({}, {});
  P->report(cout, 2);
  arr x = P->getInitializationSample();
  checkJacobianCP(*P, x, 1e-6);

  for(uint t=0;t<20;t++){
    uintA A;
    A.setRandomPerm(P->numTotalVariables());
    uint i=rnd(A.N-1);
    uint j=rnd(A.N-i);
    uintA X = A({0,i});
    uintA Y = A({i+1,i+j});
    P->subSelect(X,Y);
    P->report(cout, 2);
    x = P->getInitializationSample();
    if(!checkJacobianCP(*P, x, 1e-6)) rai::wait();
  }
}

bool NLP_GraphSolver::solveFull() {
  P->subSelect({}, {});
  std::shared_ptr<SolverReturn> ret = subSolver
                                      .setProblem(P)
                                      .solve();
  return ret->feasible;
}

bool NLP_GraphSolver::solveRandom() {
  uintA A;
  A.setRandomPerm(P->numTotalVariables());
  uint i=rnd(A.N-1);
  uint j=rnd(A.N-i);
  uintA X = A({0,i});
  uintA Y = A({i+1,i+j});
  P->subSelect(X,Y);
  P->report(cout, 2);
  std::shared_ptr<SolverReturn> ret = subSolver
                                      .setProblem(P)
                                      .solve(1);
  return ret->feasible;
}
