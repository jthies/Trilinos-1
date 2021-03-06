// @HEADER
// ************************************************************************
//
//               Rapid Optimization Library (ROL) Package
//                 Copyright (2014) Sandia Corporation
//
// Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
// license for use of this work by or on behalf of the U.S. Government.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact lead developers:
//              Drew Kouri   (dpkouri@sandia.gov) and
//              Denis Ridzal (dridzal@sandia.gov)
//
// ************************************************************************
// @HEADER

#ifndef HS_PROBLEM_026_HPP
#define HS_PROBLEM_026_HPP

#include "ROL_NonlinearProgram.hpp"

namespace HS {

namespace HS_026 {
template<class Real> 
class Obj {
public:
  template<class ScalarT>
  ScalarT value( const std::vector<ScalarT> &x, Real &tol ) {
    ScalarT a = x[0]-x[1];
    ScalarT b = x[1]-x[2];
    return a*a + b*b*b*b;
  }
};

template<class Real>
class EqCon {
public:
  template<class ScalarT> 
  void value( std::vector<ScalarT> &c,
              const std::vector<ScalarT> &x,
              Real &tol ) {
    ScalarT a = 1.0+x[1];
    c[0] = a*a*x[0] + std::pow(x[2],4)-3.0;    
  }
};
}


template<class Real> 
class Problem_026 : public ROL::NonlinearProgram<Real> {

  template<typename T> using RCP = Teuchos::RCP<T>;

  typedef ROL::NonlinearProgram<Real>   NP;
  typedef ROL::Vector<Real>             V;
  typedef ROL::Objective<Real>          OBJ;
  typedef ROL::Constraint<Real>         CON;

public:

  Problem_026() : NP( dimension_x() ) {
    NP::noBound();
  }

  int dimension_x()  { return 3; }
  int dimension_ce() { return 1; }

  const RCP<OBJ> getObjective() { 
    return Teuchos::rcp( new ROL::Sacado_StdObjective<Real,HS_026::Obj> );
  }

  const RCP<CON> getEqualityConstraint() {
    return Teuchos::rcp( 
      new ROL::Sacado_StdConstraint<Real,HS_026::EqCon> );
  }

  const RCP<const V> getInitialGuess() {
    Real x[] = {-2.6,2.0,2.0};
    return NP::createOptVector(x);
  };
   
  bool initialGuessIsFeasible() { return true; }
  
  Real getInitialObjectiveValue() { 
    return Real(21.16);
  }
 
  Real getSolutionObjectiveValue() {
    return Real(0);
  }

  RCP<const V> getSolutionSet() {
    Real alpha = std::sqrt(139.0/108.0);
    Real beta = 61.0/54.0;
    Real a = std::pow(alpha-beta,1.0/3.0) - std::pow(alpha+beta,1.0/3.0) - 2.0/3.0;
    Real x1[] = {1.0,1.0,1.0};
    Real x2[] = {a,a,a};
    return ROL::CreatePartitionedVector(NP::createOptVector(x1),
                                        NP::createOptVector(x2));
  }
 
};

} // namespace HS

#endif // HS_PROBLEM_026_HPP
