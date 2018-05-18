#include "spatial_discretization/finite_element_method/finite_element_method.h"

#include <iostream>
#include <petscksp.h>
#include <vector>
#include <numeric>

#include <Python.h>
#include "easylogging++.h"

#include "control/types.h"
#include "utility/python_utility.h"
#include "equation/type_traits.h"


namespace SpatialDiscretization
{

// set right hand side for laplace equation (set to 0)
template<typename MeshType, typename BasisFunctionType, typename QuadratureType, typename Term>
void FiniteElementMethod<MeshType, BasisFunctionType, QuadratureType, Term, Equation::hasNoRhs<Term>, BasisFunction::isNotMixed<BasisFunctionType>>::
setRightHandSide()
{
  // fill rhs vector with 0
  PetscErrorCode ierr;

  Vec &rightHandSide = this->data_.rightHandSide().values();
  ierr = VecZeroEntries(rightHandSide); CHKERRV(ierr);
}

};