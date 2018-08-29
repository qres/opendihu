#include "partition/01_mesh_partition.h"

#include "utility/vector_operators.h"
#include "basis_on_mesh/00_basis_on_mesh_base_dim.h"

namespace Partition
{
  
template<typename MeshType,typename BasisFunctionType>
MeshPartition<BasisOnMesh::BasisOnMesh<MeshType,BasisFunctionType>,Mesh::isStructured<MeshType>>::
MeshPartition(std::array<global_no_t,MeshType::dim()> nElementsGlobal, std::shared_ptr<RankSubset> rankSubset) :
  MeshPartitionBase(rankSubset), nElementsGlobal_(nElementsGlobal), hasFullNumberOfNodes_({false})
{
  VLOG(1) << "create MeshPartition where only the global size is known, " 
    << "nElementsGlobal: " << nElementsGlobal_ << ", rankSubset: " << *rankSubset << ", mesh dimension: " << MeshType::dim();
  
  this->createDmElements();
  this->createLocalDofOrderings();
  
  LOG(DEBUG) << "nElementsLocal_: " << nElementsLocal_ << ", nElementsGlobal_: " << nElementsGlobal_
    << ", hasFullNumberOfNodes_: " << hasFullNumberOfNodes_;
  LOG(DEBUG) << "nRanks: " << nRanks_ << ", localSizesOnRanks_: " << localSizesOnRanks_ << ", beginElementGlobal_: " << beginElementGlobal_;
  
  LOG(DEBUG) << *this;
}

template<typename MeshType,typename BasisFunctionType>
MeshPartition<BasisOnMesh::BasisOnMesh<MeshType,BasisFunctionType>,Mesh::isStructured<MeshType>>::
MeshPartition(std::array<node_no_t,MeshType::dim()> nElementsLocal, std::array<global_no_t,MeshType::dim()> nElementsGlobal, 
              std::array<int,MeshType::dim()> beginElementGlobal, 
              std::array<int,MeshType::dim()> nRanks, std::shared_ptr<RankSubset> rankSubset) :
  MeshPartitionBase(rankSubset), beginElementGlobal_(beginElementGlobal), nElementsLocal_(nElementsLocal), nElementsGlobal_(nElementsGlobal), 
  nRanks_(nRanks), hasFullNumberOfNodes_({false})
{
  // partitioning is already prescribed as every rank knows its own local size
 
  VLOG(1) << "create MeshPartition where every rank already knows its own local size. " 
    << "nElementsLocal: " << nElementsLocal << ", nElementsGlobal: " << nElementsGlobal 
    << ", beginElementGlobal: " << beginElementGlobal << ", nRanks: " << nRanks << ", rankSubset: " << *rankSubset;
    
  initializeHasFullNumberOfNodes();
  
  // determine localSizesOnRanks_
  for (int i = 0; i < MeshType::dim(); i++)
  {
    localSizesOnRanks_[i].resize(rankSubset->size());
  }
  
  for (int i = 0; i < MeshType::dim(); i++)
  {
    MPIUtility::handleReturnValue(MPI_Allgather(&nElementsLocal_[i], 1, MPI_INT, 
      localSizesOnRanks_[i].data(), rankSubset->size(), MPI_INT, rankSubset->mpiCommunicator()));
  }
  
  this->createLocalDofOrderings();
  
  LOG(DEBUG) << *this;
  
  for (int i = 0; i < MeshType::dim(); i++)
  {
    LOG(DEBUG) << "  beginNodeGlobal(" << i << "): " << beginNodeGlobal(i);
  }
}

template<typename MeshType,typename BasisFunctionType>
void MeshPartition<BasisOnMesh::BasisOnMesh<MeshType,BasisFunctionType>,Mesh::isStructured<MeshType>>::
initializeHasFullNumberOfNodes()
{
  // determine if the local partition is at the x+/y+/z+ border of the global domain
  for (int i = 0; i < MeshType::dim(); i++)
  {
    assert (beginElementGlobal_[i] + nElementsLocal_[i] <= nElementsGlobal_[i]);
    
    if (beginElementGlobal_[i] + nElementsLocal_[i] == nElementsGlobal_[i])
    {
      hasFullNumberOfNodes_[i] = true;
    }
  }
}

template<typename MeshType,typename BasisFunctionType>
void MeshPartition<BasisOnMesh::BasisOnMesh<MeshType,BasisFunctionType>,Mesh::isStructured<MeshType>>::
createDmElements()
{
  typedef BasisOnMesh::BasisOnMesh<MeshType,BasisFunctionType> BasisOnMeshType;
 
  dmElements_ = std::make_shared<DM>();
  
  PetscErrorCode ierr;
  const int ghostLayerWidth = 1;
  const int nDofsPerElement = 1;   // a multiplicity parameter to the items for which the partitioning is generated by PETSc. 
  
  // create PETSc DMDA object that is a topology interface handling parallel data layout on structured grids
  if (MeshType::dim() == 1)
  {
    // create 1d decomposition
    ierr = DMDACreate1d(mpiCommunicator(), DM_BOUNDARY_NONE, nElementsGlobal_[0], nDofsPerElement, ghostLayerWidth, 
                        NULL, dmElements_.get()); CHKERRV(ierr);
    
    // get global coordinates of local partition
    PetscInt x, m;
    ierr = DMDAGetCorners(*dmElements_, &x, NULL, NULL, &m, NULL, NULL); CHKERRV(ierr);
    beginElementGlobal_[0] = (global_no_t)x;
    nElementsLocal_[0] = (element_no_t)m;
    
    // get number of ranks in each coordinate direction
    ierr = DMDAGetInfo(*dmElements_, NULL, NULL, NULL, NULL, &nRanks_[0], NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL); CHKERRV(ierr);
    
    // get local sizes on the ranks
    const PetscInt *lxData;
    ierr = DMDAGetOwnershipRanges(*dmElements_, &lxData, NULL, NULL);
    
    localSizesOnRanks_[0].assign(lxData,lxData+nRanks_[0]);
  }
  else if (MeshType::dim() == 2)
  {
    // create 2d decomposition
    ierr = DMDACreate2d(mpiCommunicator(), DM_BOUNDARY_NONE, DM_BOUNDARY_NONE, DMDA_STENCIL_BOX,
                        nElementsGlobal_[0], nElementsGlobal_[1], PETSC_DECIDE, PETSC_DECIDE,
                        nDofsPerElement, ghostLayerWidth, NULL, NULL, dmElements_.get()); CHKERRV(ierr);
                        
    // get global coordinates of local partition
    PetscInt x, y, m, n;
    ierr = DMDAGetCorners(*dmElements_, &x, &y, NULL, &m, &n, NULL); CHKERRV(ierr);
    beginElementGlobal_[0] = (global_no_t)x;
    beginElementGlobal_[1] = (global_no_t)y;
    nElementsLocal_[0] = (element_no_t)m;
    nElementsLocal_[1] = (element_no_t)n;
    
    // get number of ranks in each coordinate direction
    ierr = DMDAGetInfo(*dmElements_, NULL, NULL, NULL, NULL, &nRanks_[0], &nRanks_[1], NULL, NULL, NULL, NULL, NULL, NULL, NULL); CHKERRV(ierr);
    
    // get local sizes on the ranks
    const PetscInt *lxData;
    const PetscInt *lyData;
    ierr = DMDAGetOwnershipRanges(*dmElements_, &lxData, &lyData, NULL);
    localSizesOnRanks_[0].assign(lxData, lxData + nRanks_[0]);
    localSizesOnRanks_[1].assign(lyData, lyData + nRanks_[1]);
  }
  else if (MeshType::dim() == 3)
  {
    // create 3d decomposition
    ierr = DMDACreate3d(mpiCommunicator(), DM_BOUNDARY_NONE, DM_BOUNDARY_NONE, DM_BOUNDARY_NONE, DMDA_STENCIL_BOX,
                        nElementsGlobal_[0], nElementsGlobal_[1], nElementsGlobal_[2],
                        PETSC_DECIDE, PETSC_DECIDE, PETSC_DECIDE,
                        nDofsPerElement, ghostLayerWidth, NULL, NULL, NULL, dmElements_.get()); CHKERRV(ierr);
                        
    // get global coordinates of local partition
    PetscInt x, y, z, m, n, p;
    ierr = DMDAGetCorners(*dmElements_, &x, &y, &z, &m, &n, &p); CHKERRV(ierr);
    beginElementGlobal_[0] = (global_no_t)x;
    beginElementGlobal_[1] = (global_no_t)y;
    beginElementGlobal_[2] = (global_no_t)z;
    nElementsLocal_[0] = (element_no_t)m;
    nElementsLocal_[1] = (element_no_t)n;
    nElementsLocal_[2] = (element_no_t)p;
    
    // get number of ranks in each coordinate direction
    ierr = DMDAGetInfo(*dmElements_, NULL, NULL, NULL, NULL, &nRanks_[0], &nRanks_[1], &nRanks_[2], NULL, NULL, NULL, NULL, NULL, NULL); CHKERRV(ierr);
    
    // get local sizes on the ranks
    const PetscInt *lxData;
    const PetscInt *lyData;
    const PetscInt *lzData;
    ierr = DMDAGetOwnershipRanges(*dmElements_, &lxData, &lyData, &lzData);
    localSizesOnRanks_[0].assign(lxData, lxData + nRanks_[0]);
    localSizesOnRanks_[1].assign(lyData, lyData + nRanks_[1]);
    localSizesOnRanks_[2].assign(lzData, lzData + nRanks_[2]);
  }
  
  initializeHasFullNumberOfNodes();
}
  
//! get the local to global mapping for the current partition
template<typename MeshType,typename BasisFunctionType>
ISLocalToGlobalMapping MeshPartition<BasisOnMesh::BasisOnMesh<MeshType,BasisFunctionType>,Mesh::isStructured<MeshType>>::
localToGlobalMappingDofs()
{
  return localToGlobalMappingDofs_;
}

template<typename MeshType,typename BasisFunctionType>
int MeshPartition<BasisOnMesh::BasisOnMesh<MeshType,BasisFunctionType>,Mesh::isStructured<MeshType>>::
nRanks(int coordinateDirection) const
{
  assert(0 <= coordinateDirection);
  assert(coordinateDirection < MeshType::dim());
  return nRanks_[coordinateDirection];
}

//! number of entries in the current partition
template<typename MeshType,typename BasisFunctionType>
element_no_t MeshPartition<BasisOnMesh::BasisOnMesh<MeshType,BasisFunctionType>,Mesh::isStructured<MeshType>>::
nElementsLocal() const
{
  VLOG(1) << "determine nElementsLocal";
  element_no_t result = 1;
  for (int i = 0; i < MeshType::dim(); i++)
  {
    VLOG(1) << "     * " << nElementsLocal_[i];
    result *= nElementsLocal_[i];
  }
  return result;
}

template<typename MeshType,typename BasisFunctionType>
global_no_t MeshPartition<BasisOnMesh::BasisOnMesh<MeshType,BasisFunctionType>,Mesh::isStructured<MeshType>>::
nElementsGlobal() const
{
  global_no_t result = 1;
  for (int i = 0; i < MeshType::dim(); i++)
  {
    result *= nElementsGlobal_[i];
  }
  return result;
}

template<typename MeshType,typename BasisFunctionType>
dof_no_t MeshPartition<BasisOnMesh::BasisOnMesh<MeshType,BasisFunctionType>,Mesh::isStructured<MeshType>>::
nDofsLocalWithGhosts() const
{
  const int nDofsPerNode = BasisOnMesh::BasisOnMesh<MeshType,BasisFunctionType>::nDofsPerNode();
  
  return nNodesLocalWithGhosts() * nDofsPerNode;
}

template<typename MeshType,typename BasisFunctionType>
dof_no_t MeshPartition<BasisOnMesh::BasisOnMesh<MeshType,BasisFunctionType>,Mesh::isStructured<MeshType>>::
nDofsLocalWithoutGhosts() const
{
  const int nDofsPerNode = BasisOnMesh::BasisOnMesh<MeshType,BasisFunctionType>::nDofsPerNode();

  return nNodesLocalWithoutGhosts() * nDofsPerNode;
}

template<typename MeshType,typename BasisFunctionType>
global_no_t MeshPartition<BasisOnMesh::BasisOnMesh<MeshType,BasisFunctionType>,Mesh::isStructured<MeshType>>::
nDofsGlobal() const
{
  const int nDofsPerNode = BasisOnMesh::BasisOnMesh<MeshType,BasisFunctionType>::nDofsPerNode();

  return nNodesGlobal() * nDofsPerNode;
}

//! number of nodes in the local partition
template<typename MeshType,typename BasisFunctionType>
node_no_t MeshPartition<BasisOnMesh::BasisOnMesh<MeshType,BasisFunctionType>,Mesh::isStructured<MeshType>>::
nNodesLocalWithGhosts() const
{
  element_no_t result = 1;
  for (int i = 0; i < MeshType::dim(); i++)
  {
    result *= nNodesLocalWithGhosts(i);
  }
  return result;
}

//! number of nodes in the local partition
template<typename MeshType,typename BasisFunctionType>
node_no_t MeshPartition<BasisOnMesh::BasisOnMesh<MeshType,BasisFunctionType>,Mesh::isStructured<MeshType>>::
nNodesLocalWithoutGhosts() const
{
  element_no_t result = 1;
  for (int i = 0; i < MeshType::dim(); i++)
  {
    result *= nNodesLocalWithoutGhosts(i);
  }
  return result;
}

//! number of nodes in the local partition
template<typename MeshType,typename BasisFunctionType>
node_no_t MeshPartition<BasisOnMesh::BasisOnMesh<MeshType,BasisFunctionType>,Mesh::isStructured<MeshType>>::
nNodesLocalWithGhosts(int coordinateDirection) const
{
  assert(0 <= coordinateDirection);
  assert(coordinateDirection < MeshType::dim());
  
  return this->nElementsLocal_[coordinateDirection] * BasisOnMesh::BasisOnMeshBaseDim<1,BasisFunctionType>::averageNNodesPerElement() + 1;
}

//! number of nodes in the local partition
template<typename MeshType,typename BasisFunctionType>
node_no_t MeshPartition<BasisOnMesh::BasisOnMesh<MeshType,BasisFunctionType>,Mesh::isStructured<MeshType>>::
nElementsLocal(int coordinateDirection) const
{
  assert(0 <= coordinateDirection);
  assert(coordinateDirection < MeshType::dim());
  
  return this->nElementsLocal_[coordinateDirection];
}

//! number of nodes in total
template<typename MeshType,typename BasisFunctionType>
node_no_t MeshPartition<BasisOnMesh::BasisOnMesh<MeshType,BasisFunctionType>,Mesh::isStructured<MeshType>>::
nElementsGlobal(int coordinateDirection) const
{
  assert(0 <= coordinateDirection);
  assert(coordinateDirection < MeshType::dim());
  
  return this->nElementsGlobal_[coordinateDirection];
}

//! number of nodes in the local partition
template<typename MeshType,typename BasisFunctionType>
node_no_t MeshPartition<BasisOnMesh::BasisOnMesh<MeshType,BasisFunctionType>,Mesh::isStructured<MeshType>>::
nNodesLocalWithoutGhosts(int coordinateDirection) const
{
  assert(0 <= coordinateDirection);
  assert(coordinateDirection < MeshType::dim());
  
  return this->nElementsLocal_[coordinateDirection] * BasisOnMesh::BasisOnMeshBaseDim<1,BasisFunctionType>::averageNNodesPerElement() 
    + (this->hasFullNumberOfNodes(coordinateDirection)? 1 : 0);
}

//! number of nodes in total
template<typename MeshType,typename BasisFunctionType>
global_no_t MeshPartition<BasisOnMesh::BasisOnMesh<MeshType,BasisFunctionType>,Mesh::isStructured<MeshType>>::
nNodesGlobal() const
{
  global_no_t result = 1;
  for (int i = 0; i < MeshType::dim(); i++)
  {
    result *= nNodesGlobal(i);
  }
  return result;
}

template<typename MeshType,typename BasisFunctionType>
node_no_t MeshPartition<BasisOnMesh::BasisOnMesh<MeshType,BasisFunctionType>,Mesh::isStructured<MeshType>>::
beginNodeGlobal(int coordinateDirection) const
{
  assert(0 <= coordinateDirection);
  assert(coordinateDirection < MeshType::dim());
  
  const int nNodesPer1DElement = BasisOnMesh::BasisOnMeshBaseDim<1,BasisFunctionType>::averageNNodesPerElement();
  return beginElementGlobal_[coordinateDirection] * nNodesPer1DElement;
}

//! number of nodes in total
template<typename MeshType,typename BasisFunctionType>
global_no_t MeshPartition<BasisOnMesh::BasisOnMesh<MeshType,BasisFunctionType>,Mesh::isStructured<MeshType>>::
nNodesGlobal(int coordinateDirection) const
{
  assert(0 <= coordinateDirection);
  assert(coordinateDirection < MeshType::dim());
  
  return nElementsGlobal_[coordinateDirection] * BasisOnMesh::BasisOnMeshBaseDim<1,BasisFunctionType>::averageNNodesPerElement() + 1; 
}
  
//! get if there are nodes on both borders in the given coordinate direction
//! this is the case if the local partition touches the right/top/back border
template<typename MeshType,typename BasisFunctionType>
bool MeshPartition<BasisOnMesh::BasisOnMesh<MeshType,BasisFunctionType>,Mesh::isStructured<MeshType>>::
hasFullNumberOfNodes(int coordinateDirection) const
{
  assert(0 <= coordinateDirection);
  assert(coordinateDirection < MeshType::dim());
  
  return hasFullNumberOfNodes_[coordinateDirection];
}
  
template<typename MeshType,typename BasisFunctionType>
template<typename T>
void MeshPartition<BasisOnMesh::BasisOnMesh<MeshType,BasisFunctionType>,Mesh::isStructured<MeshType>>::
extractLocalNodes(std::vector<T> &vector, int nComponents) const
{
  LOG(DEBUG) << "extractLocalNodes, nComponents=" << nComponents << ", input: " << vector;

  std::vector<T> result(nNodesLocalWithGhosts()*nComponents);
  global_no_t resultIndex = 0;
  
  if (MeshType::dim() == 1)
  {
    assert(vector.size() >= (beginNodeGlobal(0) + nNodesLocalWithGhosts(0)) * nComponents);
    for (global_no_t i = beginNodeGlobal(0); i < beginNodeGlobal(0) + nNodesLocalWithGhosts(0); i++)
    {
      for (int componentNo = 0; componentNo < nComponents; componentNo++)
      {
        result[resultIndex++] = vector[i*nComponents + componentNo];
      }
    }
  }
  else if (MeshType::dim() == 2)
  {
    assert(vector.size() >= ((beginNodeGlobal(1) + nNodesLocalWithGhosts(1) - 1)*nNodesGlobal(0) + beginNodeGlobal(0) + nNodesLocalWithGhosts(0)) * nComponents);
    for (global_no_t j = beginNodeGlobal(1); j < beginNodeGlobal(1) + nNodesLocalWithGhosts(1); j++)
    {
      for (global_no_t i = beginNodeGlobal(0); i < beginNodeGlobal(0) + nNodesLocalWithGhosts(0); i++)
      {
        for (int componentNo = 0; componentNo < nComponents; componentNo++)
        {
          result[resultIndex++] = vector[(j*nNodesGlobal(0) + i)*nComponents + componentNo];
        }
      }
    }
  }
  else if (MeshType::dim() == 3)
  {
    assert(vector.size() >= ((beginNodeGlobal(2) + nNodesLocalWithGhosts(2) - 1)*nNodesGlobal(1)*nNodesGlobal(0)
      + (beginNodeGlobal(1) + nNodesLocalWithGhosts(1) - 1)*nNodesGlobal(0) + beginNodeGlobal(0) + nNodesLocalWithGhosts(0)) * nComponents);
    for (global_no_t k = beginNodeGlobal(2); k < beginNodeGlobal(2) + nNodesLocalWithGhosts(2); k++)
    {
      for (global_no_t j = beginNodeGlobal(1); j < beginNodeGlobal(1) + nNodesLocalWithGhosts(1); j++)
      {
        for (global_no_t i = beginNodeGlobal(0); i < beginNodeGlobal(0) + nNodesLocalWithGhosts(0); i++)
        {
          for (int componentNo = 0; componentNo < nComponents; componentNo++)
          {
            result[resultIndex++] = vector[(k*nNodesGlobal(1)*nNodesGlobal(0) + j*nNodesGlobal(0) + i)*nComponents + componentNo];
          }
        }
      }
    }
  }
  LOG(DEBUG) << "                   output: " << result;
  
  // store values
  vector.assign(result.begin(), result.end());
}
  
template<typename MeshType,typename BasisFunctionType>
void MeshPartition<BasisOnMesh::BasisOnMesh<MeshType,BasisFunctionType>,Mesh::isStructured<MeshType>>::
extractLocalDofsWithoutGhosts(std::vector<double> &vector) const 
{
  dof_no_t nDofsPerNode = BasisOnMesh::BasisOnMesh<MeshType,BasisFunctionType>::nDofsPerNode();
  std::vector<double> result(nDofsLocalWithoutGhosts());
  global_no_t resultIndex = 0;
  
  if (MeshType::dim() == 1)
  {
    assert(vector.size() >= beginNodeGlobal(0) + nNodesLocalWithGhosts(0));
    for (global_no_t i = beginNodeGlobal(0); i < beginNodeGlobal(0) + nNodesLocalWithoutGhosts(0); i++)
    {
      for (int dofOnNodeIndex = 0; dofOnNodeIndex < nDofsPerNode; dofOnNodeIndex++)
      {
        result[resultIndex++] = vector[i*nDofsPerNode + dofOnNodeIndex];
      }
    }
  }
  else if (MeshType::dim() == 2)
  {
    for (global_no_t j = beginNodeGlobal(1); j < beginNodeGlobal(1) + nNodesLocalWithoutGhosts(1); j++)
    {
      for (global_no_t i = beginNodeGlobal(0); i < beginNodeGlobal(0) + nNodesLocalWithoutGhosts(0); i++)
      {
        for (int dofOnNodeIndex = 0; dofOnNodeIndex < nDofsPerNode; dofOnNodeIndex++)
        {
          result[resultIndex++] = vector[(j*nNodesGlobal(0) + i)*nDofsPerNode + dofOnNodeIndex];
        }
      }
    }
  }
  else if (MeshType::dim() == 3)
  {
    for (global_no_t k = beginNodeGlobal(2); k < beginNodeGlobal(2) + nNodesLocalWithoutGhosts(2); k++)
    {
      for (global_no_t j = beginNodeGlobal(1); j < beginNodeGlobal(1) + nNodesLocalWithoutGhosts(1); j++)
      {
        for (global_no_t i = beginNodeGlobal(0); i < beginNodeGlobal(0) + nNodesLocalWithoutGhosts(0); i++)
        {
          for (int dofOnNodeIndex = 0; dofOnNodeIndex < nDofsPerNode; dofOnNodeIndex++)
          {
            result[resultIndex++] = vector[(k*nNodesGlobal(1)*nNodesGlobal(0) + j*nNodesGlobal(0) + i)*nDofsPerNode + dofOnNodeIndex];
          }
        }
      }
    }
  }
  
  // store values
  vector.assign(result.begin(), result.end());
}

//! get a vector of local dof nos, range [0,nDofsLocalWithoutGhosts] are the dofs without ghost dofs, the whole vector are the dofs with ghost dofs
//! @param onlyNodalValues: if for Hermite only get every second dof such that derivatives are not returned
template<typename MeshType,typename BasisFunctionType>
const std::vector<PetscInt> &MeshPartition<BasisOnMesh::BasisOnMesh<MeshType,BasisFunctionType>,Mesh::isStructured<MeshType>>::
dofNosLocal(bool onlyNodalValues) const
{
  if (onlyNodalValues)
  {
    return onlyNodalDofLocalNos_;
  }
  else 
  {
    return this->dofNosLocal_;
  }
}

template<typename MeshType,typename BasisFunctionType>
const std::vector<PetscInt> &MeshPartition<BasisOnMesh::BasisOnMesh<MeshType,BasisFunctionType>,Mesh::isStructured<MeshType>>::
ghostDofGlobalNos() const
{
  return ghostDofGlobalNos_;
}

//! fill the dofLocalNo vectors
template<typename MeshType,typename BasisFunctionType>
void MeshPartition<BasisOnMesh::BasisOnMesh<MeshType,BasisFunctionType>,Mesh::isStructured<MeshType>>::
createLocalDofOrderings()
{
  LOG(DEBUG) << "call createLocalDofOrderings of child";
  MeshPartitionBase::createLocalDofOrderings(nDofsLocalWithGhosts());
    
  // fill onlyNodalDofLocalNos_
  const int nDofsPerNode = BasisOnMesh::BasisOnMesh<MeshType,BasisFunctionType>::nDofsPerNode();
  int nNodalDofs = nDofsLocalWithGhosts() / nDofsPerNode;
  onlyNodalDofLocalNos_.resize(nNodalDofs);
  
  dof_no_t localDofNo = 0;
  for (node_no_t localNodeNo = 0; localNodeNo < nNodesLocalWithGhosts(); localNodeNo++)
  {
    onlyNodalDofLocalNos_[localNodeNo] = localDofNo;
    localDofNo += nDofsPerNode;
  }
  
  // fill ghostDofGlobalNos_
  int resultIndex = 0;
  int nGhostDofss = nDofsLocalWithGhosts() - nDofsLocalWithoutGhosts();
  ghostDofGlobalNos_.resize(nGhostDofss);
  if (MeshType::dim() == 1)
  {
    for (global_no_t i = beginNodeGlobal(0) + nNodesLocalWithoutGhosts(0); i < beginNodeGlobal(0) + nNodesLocalWithGhosts(0); i++)
    {
      for (int dofOnNodeIndex = 0; dofOnNodeIndex < nDofsPerNode; dofOnNodeIndex++)
      {
        ghostDofGlobalNos_[resultIndex++] = i*nDofsPerNode + dofOnNodeIndex;
      }
    }
  }
  else if (MeshType::dim() == 2)
  {
    for (global_no_t j = beginNodeGlobal(1) + nNodesLocalWithoutGhosts(1); j < beginNodeGlobal(1) + nNodesLocalWithGhosts(1); j++)
    {
      for (global_no_t i = beginNodeGlobal(0) + nNodesLocalWithoutGhosts(0); i < beginNodeGlobal(0) + nNodesLocalWithGhosts(0); i++)
      {
        for (int dofOnNodeIndex = 0; dofOnNodeIndex < nDofsPerNode; dofOnNodeIndex++)
        {
          ghostDofGlobalNos_[resultIndex++] = (j*nNodesGlobal(0) + i)*nDofsPerNode + dofOnNodeIndex;
        }
      }
    }
  }
  else if (MeshType::dim() == 3)
  {
    for (global_no_t k = beginNodeGlobal(2) + nNodesLocalWithoutGhosts(2); k < beginNodeGlobal(2) + nNodesLocalWithGhosts(2); k++)
    {
      for (global_no_t j = beginNodeGlobal(1) + nNodesLocalWithoutGhosts(1); j < beginNodeGlobal(1) + nNodesLocalWithGhosts(1); j++)
      {
        for (global_no_t i = beginNodeGlobal(0) + nNodesLocalWithoutGhosts(0); i < beginNodeGlobal(0) + nNodesLocalWithGhosts(0); i++)
        {
          for (int dofOnNodeIndex = 0; dofOnNodeIndex < nDofsPerNode; dofOnNodeIndex++)
          {
            ghostDofGlobalNos_[resultIndex++] = (k*nNodesGlobal(1)*nNodesGlobal(0) + j*nNodesGlobal(0) + i)*nDofsPerNode + dofOnNodeIndex;
          }
        }
      }
    }
  }
  
  // create localToGlobalMappingDofs_
  PetscErrorCode ierr;
  Vec temporaryVector;
  ierr = VecCreateGhost(this->mpiCommunicator(), nDofsLocalWithoutGhosts(),
                        nDofsGlobal(), nGhostDofss, ghostDofGlobalNos_.data(), &temporaryVector); CHKERRV(ierr);
  
  // retrieve local to global mapping
  ierr = VecGetLocalToGlobalMapping(temporaryVector, &localToGlobalMappingDofs_); CHKERRV(ierr);
  //ierr = VecDestroy(&temporaryVector); CHKERRV(ierr);
}

template<typename MeshType,typename BasisFunctionType>
global_no_t MeshPartition<BasisOnMesh::BasisOnMesh<MeshType,BasisFunctionType>,Mesh::isStructured<MeshType>>::
getElementNoGlobal(element_no_t elementNoLocal) const
{
  if (MeshType::dim() == 1)
  {
    return beginElementGlobal_[0] + elementNoLocal;
  }
  else if (MeshType::dim() == 2)
  {
    element_no_t elementY = element_no_t(elementNoLocal / nElementsLocal(0));
    element_no_t elementX = elementNoLocal % nElementsLocal(0);

    return (beginElementGlobal_[1] + elementY) * nElementsGlobal_[0]
      + beginElementGlobal_[0] + elementX;
  }
  else if (MeshType::dim() == 3)
  {
    element_no_t elementZ = element_no_t(elementNoLocal / (nElementsLocal(0) * nElementsLocal(1)));
    element_no_t elementY = element_no_t((elementNoLocal % (nElementsLocal(0) * nElementsLocal(1))) / nElementsLocal(0));
    element_no_t elementX = elementNoLocal % nElementsLocal(0);

    return (beginElementGlobal_[2] + elementZ) * nElementsGlobal_[0] * nElementsGlobal_[1]
      + (beginElementGlobal_[1] + elementY) * nElementsGlobal_[0]
      + beginElementGlobal_[0] + elementX;
  }
}

template<typename MeshType,typename BasisFunctionType>
void MeshPartition<BasisOnMesh::BasisOnMesh<MeshType,BasisFunctionType>,Mesh::isStructured<MeshType>>::
output(std::ostream &stream)
{
  stream << "MeshPartition<structured>, nElementsGlobal: ";
  for (int i = 0; i < MeshType::dim(); i++)
    stream << nElementsGlobal_[i] << ",";
  
  stream << " nElementsLocal: ";
  for (int i = 0; i < MeshType::dim(); i++)
    stream << nElementsLocal_[i] << ",";
  
  stream << " nRanks: ";
  for (int i = 0; i < MeshType::dim(); i++)
    stream << nRanks_[i] << ",";
  
  stream << " beginElementGlobal: ";
  for (int i = 0; i < MeshType::dim(); i++)
    stream << beginElementGlobal_[i] << ",";
  
  stream << " hasFullNumberOfNodes: " ;
  for (int i = 0; i < MeshType::dim(); i++)
    stream << hasFullNumberOfNodes_[i] << ",";
  
  stream << " localSizesOnRanks: ";
  for (int i = 0; i < MeshType::dim(); i++)
    stream << localSizesOnRanks_[i] << ",";
  
  stream << " nNodesGlobal: ";
  for (int i = 0; i < MeshType::dim(); i++)
    stream << nNodesGlobal(i) << ",";
  stream << "total " << nNodesGlobal() << ",";
  
  stream << " nNodesLocalWithGhosts: ";
  for (int i = 0; i < MeshType::dim(); i++)
    stream << nNodesLocalWithGhosts(i) << ",";
  stream << "total " << nNodesLocalWithGhosts() << ",";
  
  stream << " nNodesLocalWithoutGhosts: ";
  for (int i = 0; i < MeshType::dim(); i++)
    stream << nNodesLocalWithoutGhosts(i) << ",";
  stream << "total " << nNodesLocalWithoutGhosts()
    << ", dofNosLocal: [";
  for (int i = 0; i < this->dofNosLocal_.size(); i++)
    stream << this->dofNosLocal_[i] << " ";
  stream << "], ghostDofGlobalNos: [";
    
  for (int i = 0; i < ghostDofGlobalNos_.size(); i++)
    stream << ghostDofGlobalNos_[i] << " ";
  stream << "], localToGlobalMappingDofs_: " << localToGlobalMappingDofs_;
} 

}  // namespace
