#pragma once

#include "utility/type_utility.h"

#include <cstdlib>

/** The functions in this file model a loop over the nodeents of a tuple, as it occurs as OutputFieldVariablesType in all data_management classes.
 *  (Because the types inside the tuple are static and fixed at compile-time, a simple for loop c not work here.)
 *  The two functions starting with loop recursively emulate the loop. One method is the break condition and does nothing, the other method does the work and calls the method without loop in the name.
 *  OutputFieldVariablesType is assumed to be of type std::tuple<...>> where the types can be (mixed) std::shared_ptr<FieldVariable> or std::vector<std::shared_ptr<FieldVariable>>.
 * 
 *  Collect all field variable names of field variable with > 1 components into vectors and all scalar field variable names into scalars.
 */

namespace OutputWriter
{

namespace ParaviewLoopOverTuple
{
 
 /** Static recursive loop from 0 to number of entries in the tuple
 *  Stopping criterion
 */
template<typename OutputFieldVariablesType, int i=0>
inline typename std::enable_if<i == std::tuple_size<OutputFieldVariablesType>::value, void>::type
loopCollectFieldVariablesNames(const OutputFieldVariablesType &fieldVariables, std::string meshName,
                               std::vector<std::string> &scalars, std::vector<std::string> &vectors
)
{}

 /** Static recursive loop from 0 to number of entries in the tuple
 * Loop body
 */
template<typename OutputFieldVariablesType, int i=0>
inline typename std::enable_if<i < std::tuple_size<OutputFieldVariablesType>::value, void>::type
loopCollectFieldVariablesNames(const OutputFieldVariablesType &fieldVariables, std::string meshName, 
                               std::vector<std::string> &scalars, std::vector<std::string> &vectors);


/** Loop body for a vector nodeent
 */
template<typename VectorType, typename OutputFieldVariablesType>
typename std::enable_if<TypeUtility::isVector<VectorType>::value, bool>::type
collectFieldVariablesNames(VectorType currentFieldVariableVector, const OutputFieldVariablesType &fieldVariables, std::string meshName, 
                           std::vector<std::string> &scalars, std::vector<std::string> &vectors);

 /**  Loop body for a pointer nodeent
 */
template<typename CurrentFieldVariableType, typename OutputFieldVariablesType>
typename std::enable_if<!TypeUtility::isVector<CurrentFieldVariableType>::value, bool>::type
collectFieldVariablesNames(CurrentFieldVariableType currentFieldVariable, const OutputFieldVariablesType &fieldVariables, std::string meshName, 
                           std::vector<std::string> &scalars, std::vector<std::string> &vectors);

};  //namespace ParaviewLoopOverTuple

};  //namespace OutputWriter

#include "output_writer/paraview/loop_collect_field_variables_names.tpp"