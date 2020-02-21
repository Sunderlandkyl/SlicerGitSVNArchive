/*==============================================================================

  Copyright (c) Laboratory for Percutaneous Surgery (PerkLab)
  Queen's University, Kingston, ON, Canada. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Kyle Sunderland, PerkLab, Queen's University
  and was supported through CANARIE's Research Software Program, Cancer
  Care Ontario, OpenAnatomy, and Brigham and Women’s Hospital through NIH grant R01MH112748.

==============================================================================*/

#ifndef __vtkSlicerDijkstraGraphGeodesicPath_h
#define __vtkSlicerDijkstraGraphGeodesicPath_h

// VTK includes
#include <vtkDijkstraGraphGeodesicPath.h>

// export
#include "vtkSlicerMarkupsModuleMRMLExport.h"

//
// Set built-in type.  Creates member Set"name"() (e.g., SetVisibility());
//
#define vtkSetAdjacencyMacro(name,type) \
virtual void Set##name (type _arg) \
{ \
  vtkDebugMacro(<< this->GetClassName() << " (" << this << "): setting " #name " to " << _arg); \
  if (this->name != _arg) \
  { \
    this->RecalculateAdjacency = true;\
    this->name = _arg; \
    this->Modified(); \
  } \
}

/// Filter that generates curves between points of an input polydata
class VTK_SLICER_MARKUPS_MODULE_MRML_EXPORT vtkSlicerDijkstraGraphGeodesicPath : public vtkDijkstraGraphGeodesicPath
{
public:
  vtkTypeMacro(vtkSlicerDijkstraGraphGeodesicPath, vtkDijkstraGraphGeodesicPath);
  static vtkSlicerDijkstraGraphGeodesicPath* New();
  void PrintSelf(ostream& os, vtkIndent indent) override;

  enum
  {
    COST_FUNCTION_DISTANCE,
    COST_FUNCTION_ADDITIVE,
    COST_FUNCTION_INVERSE_SQUARED,
    COST_FUNCTION_LAST,
  };
  static const char* GetCostFunctionAsString(int costFunction);
  static const char* GetCostFunctionAsHumanReadableString(int costFunction);
  vtkSetAdjacencyMacro(CostFunction, int);
  vtkGetMacro(CostFunction, int);
  vtkSetAdjacencyMacro(UseScalarWeights, vtkTypeBool);

protected:

  int RequestData(vtkInformation*, vtkInformationVector**,
    vtkInformationVector*) override;

  // Build a graph description of the input.
  virtual void BuildAdjacency(vtkDataSet* inData);

  // The fixed cost going from vertex u to v.
  virtual double CalculateStaticEdgeCost(vtkDataSet* inData, vtkIdType u, vtkIdType v);

  bool RecalculateAdjacency;
  int CostFunction;

protected:
  vtkSlicerDijkstraGraphGeodesicPath();
  ~vtkSlicerDijkstraGraphGeodesicPath() override;
  vtkSlicerDijkstraGraphGeodesicPath(const vtkSlicerDijkstraGraphGeodesicPath&) = delete;
  void operator=(const vtkSlicerDijkstraGraphGeodesicPath&) = delete;
};

#endif
