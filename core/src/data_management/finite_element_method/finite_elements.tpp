#include "data_management/finite_element_method/finite_elements.h"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>
#include <memory>

#include "easylogging++.h"

#include "utility/python_utility.h"
#include "control/dihu_context.h"
#include "utility/petsc_utility.h"
#include "function_space/function_space.h"
#include "mesh/unstructured_deformable.h"
#include "basis_function/hermite.h"
#include "partition/partitioned_petsc_mat/partitioned_petsc_mat.h"

namespace Data
{

//! constructor
template<typename FunctionSpaceType,int nComponents>
FiniteElements<FunctionSpaceType,nComponents,Equation::Dynamic::AnisotropicDiffusion>::
FiniteElements(DihuContext context) :
  FiniteElementsBase<FunctionSpaceType,nComponents>(context),
  DiffusionTensorConstant<FunctionSpaceType>(context.getPythonConfig())
{
}

//! constructor
template<typename FunctionSpaceType,int nComponents>
FiniteElements<FunctionSpaceType,nComponents,Equation::Dynamic::DirectionalDiffusion>::
FiniteElements(DihuContext context) :
  FiniteElementsBase<FunctionSpaceType,nComponents>(context),
  DiffusionTensorDirectional<FunctionSpaceType>(context.getPythonConfig())
{
}

template<typename FunctionSpaceType,int nComponents>
void FiniteElements<FunctionSpaceType,nComponents,Equation::Dynamic::AnisotropicDiffusion>::
initialize()
{
  LOG(DEBUG) << "Data::FiniteElements::initialize";

  FiniteElementsBase<FunctionSpaceType,nComponents>::initialize();

  // set up diffusion tensor if there is any
  DiffusionTensorConstant<FunctionSpaceType>::initialize();
}

template<typename FunctionSpaceType,int nComponents>
void FiniteElements<FunctionSpaceType,nComponents,Equation::Dynamic::DirectionalDiffusion>::
initialize(std::shared_ptr<FieldVariable::FieldVariable<FunctionSpaceType,3>> direction,
           std::shared_ptr<FieldVariable::FieldVariable<FunctionSpaceType,1>> spatiallyVaryingPrefactor,
           bool useAdditionalDiffusionTensor)
{
  FiniteElementsBase<FunctionSpaceType,nComponents>::initialize();

  // set up diffusion tensor, initialize with given direction field
  DiffusionTensorDirectional<FunctionSpaceType>::initialize(direction, spatiallyVaryingPrefactor, useAdditionalDiffusionTensor);
}

//! initialize, store the reference geometry as copy of the current geometry
template<typename FunctionSpaceType,int nComponents>
void FiniteElements<FunctionSpaceType,nComponents,Equation::Static::LinearElasticity>::
initialize()
{
  LinearStiffness<FunctionSpaceType,nComponents>::initialize();

  referenceGeometry_ = std::make_shared<FieldVariable::FieldVariable<FunctionSpaceType,3>>(this->functionSpace_->geometryField());
}

//! update the geometry of the mesh and function space with the displacements
template<typename FunctionSpaceType,int nComponents>
void FiniteElements<FunctionSpaceType,nComponents,Equation::Static::LinearElasticity>::
updateGeometry()
{
  PetscErrorCode ierr;

  // w = alpha * x + y, VecWAXPY(w, alpha, x, y)
  ierr = VecWAXPY(this->functionSpace_->geometryField()->valuesGlobal(), 1, this->referenceGeometry_->valuesGlobal(), this->solution_->valuesGlobal());
}

} // namespace Data
