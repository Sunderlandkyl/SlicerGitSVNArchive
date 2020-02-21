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

// Markups MRML includes
#include "vtkSlicerDijkstraGraphGeodesicPath.h"

// VTK includes
#include <vtkFloatArray.h>
#include <vtkPointData.h>
#include <vtkInformation.h>
#include <vtkInformationVector.h>

//------------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerDijkstraGraphGeodesicPath);

//------------------------------------------------------------------------------
vtkSlicerDijkstraGraphGeodesicPath::vtkSlicerDijkstraGraphGeodesicPath()
{
  this->UseScalarWeights = true;
  this->RecalculateAdjacency = true;
  this->CostFunction = COST_FUNCTION_DISTANCE;
}

//------------------------------------------------------------------------------
vtkSlicerDijkstraGraphGeodesicPath::~vtkSlicerDijkstraGraphGeodesicPath()
= default;

//------------------------------------------------------------------------------
void vtkSlicerDijkstraGraphGeodesicPath::PrintSelf(std::ostream &os, vtkIndent indent)
{
  Superclass::PrintSelf(os, indent);
  os << indent << "CostFunction: " << this->GetCostFunctionAsString(this->CostFunction) << std::endl;
}

//------------------------------------------------------------------------------
const char* vtkSlicerDijkstraGraphGeodesicPath::GetCostFunctionAsString(int costFunction)
{
  switch (costFunction)
    {
    case COST_FUNCTION_DISTANCE:
      {
      return "distance";
      }
    case COST_FUNCTION_ADDITIVE:
      {
      return "additive";
      }
    case COST_FUNCTION_INVERSE_SQUARED:
      {
      return "inverseSquared";
      }
    default:
      {
      return "";
      }
    }
}

//------------------------------------------------------------------------------
const char* vtkSlicerDijkstraGraphGeodesicPath::GetCostFunctionAsHumanReadableString(int costFunction)
{
  switch (costFunction)
    {
    case COST_FUNCTION_DISTANCE:
      {
      return "Distance";
      }
    case COST_FUNCTION_ADDITIVE:
      {
      return "Additive";
      }
    case COST_FUNCTION_INVERSE_SQUARED:
      {
      return "Inverse squared";
      }
    default:
      {
      return "";
      }
    }
}

//----------------------------------------------------------------------------
int vtkSlicerDijkstraGraphGeodesicPath::RequestData(
  vtkInformation* vtkNotUsed(request),
  vtkInformationVector** inputVector,
  vtkInformationVector* outputVector)
{
  vtkInformation* inInfo = inputVector[0]->GetInformationObject(0);
  vtkInformation* outInfo = outputVector->GetInformationObject(0);

  vtkPolyData* input = vtkPolyData::SafeDownCast(
    inInfo->Get(vtkDataObject::DATA_OBJECT()));
  if (!input)
    {
    return 0;
    }

  vtkPolyData* output = vtkPolyData::SafeDownCast(
    outInfo->Get(vtkDataObject::DATA_OBJECT()));
  if (!output)
    {
    return 0;
    }

  if (this->AdjacencyBuildTime.GetMTime() < input->GetMTime() || this->RecalculateAdjacency)
    {
    this->Initialize(input);
    }
  else
    {
    this->Reset();
    }

  if (this->NumberOfVertices == 0)
    {
    return 0;
    }

  this->ShortestPath(input, this->StartVertex, this->EndVertex);
  this->TraceShortestPath(input, output, this->StartVertex, this->EndVertex);
  return 1;
}

//------------------------------------------------------------------------------
void vtkSlicerDijkstraGraphGeodesicPath::BuildAdjacency(vtkDataSet* inData)
{
  Superclass::BuildAdjacency(inData);
  this->RecalculateAdjacency = false;
}

//------------------------------------------------------------------------------
double vtkSlicerDijkstraGraphGeodesicPath::CalculateStaticEdgeCost(vtkDataSet* inData, vtkIdType u, vtkIdType v)
{
  // Parent implementation is inverse squared
  if (this->CostFunction == COST_FUNCTION_INVERSE_SQUARED)
    {
    return Superclass::CalculateStaticEdgeCost(inData, u, v);
    }

  double p1[3];
  inData->GetPoint(u,p1);
  double p2[3];
  inData->GetPoint(v,p2);

  double distance = sqrt(vtkMath::Distance2BetweenPoints(p1, p2));
  double cost = distance;
  if (this->UseScalarWeights && this->CostFunction != COST_FUNCTION_DISTANCE)
    {
    double scalarV = 0.0;
    // Note this edge cost is not symmetric!
    if (inData->GetPointData())
      {
      vtkFloatArray* scalars = vtkFloatArray::SafeDownCast(inData->GetPointData()->GetScalars());
      if (scalars)
        {
        scalarV = static_cast<double>(scalars->GetValue(v));
        }
      }

    switch (this->CostFunction)
      {
      case COST_FUNCTION_ADDITIVE:
        cost += scalarV;
        break;
      default:
        break;
      }
    }
  return cost;
}
