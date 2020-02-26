/*=========================================================================

 Copyright (c) ProxSim ltd., Kwun Tong, Hong Kong. All Rights Reserved.

 See COPYRIGHT.txt
 or http://www.slicer.org/copyright/copyright.txt for details.

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.

 This file was originally developed by Davide Punzo, punzodavide@hotmail.it,
 and development was supported by ProxSim ltd.

=========================================================================*/

// VTK includes
#include "vtkActor2D.h"
#include "vtkArcSource.h"
#include "vtkCellLocator.h"
#include "vtkDiscretizableColorTransferFunction.h"
#include "vtkLine.h"
#include "vtkMath.h"
#include "vtkMatrix4x4.h"
#include "vtkObjectFactory.h"
#include "vtkPlane.h"
#include "vtkPoints.h"
#include "vtkPointData.h"
#include "vtkPolyData.h"
#include "vtkPolyDataMapper2D.h"
#include "vtkProperty2D.h"
#include "vtkRenderer.h"
#include "vtkSampleImplicitFunctionFilter.h"
#include "vtkSlicerPlaneRepresentation2D.h"
#include "vtkTextActor.h"
#include "vtkTextProperty.h"
#include "vtkTransform.h"
#include "vtkTransformPolyDataFilter.h"
#include "vtkPlaneSource.h"

// MRML includes
#include "vtkMRMLInteractionEventData.h"
#include "vtkMRMLMarkupsDisplayNode.h"
#include "vtkMRMLProceduralColorNode.h"


vtkStandardNewMacro(vtkSlicerPlaneRepresentation2D);

//----------------------------------------------------------------------
vtkSlicerPlaneRepresentation2D::vtkSlicerPlaneRepresentation2D()
{
  this->PlaneSliceDistance = vtkSmartPointer<vtkSampleImplicitFunctionFilter>::New();
  this->PlaneSliceDistance->SetImplicitFunction(this->SlicePlane);

  this->PlaneWorldToSliceTransformer = vtkSmartPointer<vtkTransformPolyDataFilter>::New();

  this->PlaneWorldToSliceTransformer->SetTransform(this->WorldToSliceTransform);

  this->PlaneWorldToSliceTransformer->SetInputConnection(this->PlaneSliceDistance->GetOutputPort());

  this->PlaneFilter = vtkSmartPointer<vtkPlaneSource>::New();
  this->PlaneFilter->SetInputConnection(this->PlaneWorldToSliceTransformer->GetOutputPort());

  this->ColorMap = vtkSmartPointer<vtkDiscretizableColorTransferFunction>::New();

  this->PlaneMapper = vtkSmartPointer<vtkPolyDataMapper2D>::New();
  this->PlaneMapper->SetInputConnection(this->PlaneFilter->GetOutputPort());
  this->PlaneMapper->SetLookupTable(this->ColorMap);
  this->PlaneMapper->SetScalarVisibility(true);

  this->PlaneActor = vtkSmartPointer<vtkActor2D>::New();
  this->PlaneActor->SetMapper(this->PlaneMapper);
  this->PlaneActor->SetProperty(this->GetControlPointsPipeline(Unselected)->Property);

  this->LabelFormat = "%s: %-#6.3g";
}

//----------------------------------------------------------------------
vtkSlicerPlaneRepresentation2D::~vtkSlicerPlaneRepresentation2D()
= default;

//----------------------------------------------------------------------
bool vtkSlicerPlaneRepresentation2D::GetTransformationReferencePoint(double referencePointWorld[3])
{
  vtkMRMLMarkupsNode* markupsNode = this->GetMarkupsNode();
  if (!markupsNode || markupsNode->GetNumberOfControlPoints() < 2)
    {
    return false;
    }
  markupsNode->GetNthControlPointPositionWorld(1, referencePointWorld);
  return true;
}

//----------------------------------------------------------------------
void vtkSlicerPlaneRepresentation2D::UpdateFromMRML(vtkMRMLNode* caller, unsigned long event, void *callData /*=nullptr*/)
{
  Superclass::UpdateFromMRML(caller, event, callData);

  this->NeedToRenderOn();

  vtkMRMLMarkupsNode* markupsNode = this->GetMarkupsNode();
  if (!markupsNode || !this->MarkupsDisplayNode
    || !this->MarkupsDisplayNode->GetVisibility()
    || !this->MarkupsDisplayNode->IsDisplayableInView(this->ViewNode->GetID())
    )
    {
    this->VisibilityOff();
    return;
    }

  this->VisibilityOn();

  // Update plane display properties
  this->PlaneActor->SetVisibility(markupsNode->GetNumberOfControlPoints() >= 2);

  int controlPointType = Unselected;
  if (this->MarkupsDisplayNode->GetActiveComponentType() == vtkMRMLMarkupsDisplayNode::ComponentLine)
    {
    controlPointType = Active;
    }
  else if ((markupsNode->GetNumberOfControlPoints() > 0 && !markupsNode->GetNthControlPointSelected(0)) ||
           (markupsNode->GetNumberOfControlPoints() > 1 && !markupsNode->GetNthControlPointSelected(1)) ||
           (markupsNode->GetNumberOfControlPoints() > 2 && !markupsNode->GetNthControlPointSelected(2)))
    {
    controlPointType = Unselected;
    }
  else
    {
    controlPointType = Selected;
    }
  this->PlaneActor->SetProperty(this->GetControlPointsPipeline(controlPointType)->Property);
  this->TextActor->SetTextProperty(this->GetControlPointsPipeline(controlPointType)->TextProperty);

  if (this->MarkupsDisplayNode->GetLineColorNode() && this->MarkupsDisplayNode->GetLineColorNode()->GetColorTransferFunction())
    {
    // Update the line color mapping from the colorNode stored in the markups display node
    vtkColorTransferFunction* colormap = this->MarkupsDisplayNode->GetLineColorNode()->GetColorTransferFunction();
    this->PlaneMapper->SetLookupTable(colormap);
    }
  else
    {
    // if there is no line color node, build the color mapping from few varibales
    // (color, opacity, distance fading, saturation and hue offset) stored in the display node
    this->UpdateDistanceColorMap(this->ColorMap, this->PlaneActor->GetProperty()->GetColor());
    this->PlaneMapper->SetLookupTable(this->ColorMap);
    }
}

//----------------------------------------------------------------------
void vtkSlicerPlaneRepresentation2D::CanInteract(
  vtkMRMLInteractionEventData* interactionEventData,
  int &foundComponentType, int &foundComponentIndex, double &closestDistance2)
{
  foundComponentType = vtkMRMLMarkupsDisplayNode::ComponentNone;
  vtkMRMLMarkupsNode* markupsNode = this->GetMarkupsNode();
  if ( !markupsNode || markupsNode->GetLocked() || markupsNode->GetNumberOfControlPoints() < 1
    || !this->GetVisibility() || !interactionEventData )
    {
    return;
    }
  Superclass::CanInteract(interactionEventData, foundComponentType, foundComponentIndex, closestDistance2);
  if (foundComponentType != vtkMRMLMarkupsDisplayNode::ComponentNone)
    {
    // if mouse is near a control point then select that (ignore the line)
    return;
    }

  const int* displayPosition = interactionEventData->GetDisplayPosition();
  double displayPosition3[3] = { static_cast<double>(displayPosition[0]), static_cast<double>(displayPosition[1]), 0.0 };

  double maxPickingDistanceFromControlPoint2 = this->GetMaximumControlPointPickingDistance2();

  vtkIdType numberOfPoints = markupsNode->GetNumberOfControlPoints();

  double pointDisplayPos1[4] = { 0.0, 0.0, 0.0, 1.0 };
  double pointWorldPos1[4] = { 0.0, 0.0, 0.0, 1.0 };
  double pointDisplayPos2[4] = { 0.0, 0.0, 0.0, 1.0 };
  double pointWorldPos2[4] = { 0.0, 0.0, 0.0, 1.0 };

  vtkNew<vtkMatrix4x4> rasToxyMatrix;
  vtkMatrix4x4::Invert(this->GetSliceNode()->GetXYToRAS(), rasToxyMatrix.GetPointer());
  for (int i = 0; i < numberOfPoints-1; i++)
    {
    if (!this->PointsVisibilityOnSlice->GetValue(i))
      {
      continue;
      }
    if (!this->PointsVisibilityOnSlice->GetValue(i+1))
      {
      i++; // skip one more, as the next iteration would use (i+1)-th point
      continue;
      }
    markupsNode->GetNthControlPointPositionWorld(i, pointWorldPos1);
    rasToxyMatrix->MultiplyPoint(pointWorldPos1, pointDisplayPos1);
    markupsNode->GetNthControlPointPositionWorld(i+1, pointWorldPos2);
    rasToxyMatrix->MultiplyPoint(pointWorldPos2, pointDisplayPos2);

    double relativePositionAlongLine = -1.0; // between 0.0-1.0 if between the endpoints of the line segment
    double distance2 = vtkLine::DistanceToLine(displayPosition3, pointDisplayPos1, pointDisplayPos2, relativePositionAlongLine);
    if (distance2 < maxPickingDistanceFromControlPoint2 && distance2 < closestDistance2 && relativePositionAlongLine >= 0 && relativePositionAlongLine <= 1)
      {
      closestDistance2 = distance2;
      foundComponentType = vtkMRMLMarkupsDisplayNode::ComponentLine;
      foundComponentIndex = i;
      }
    }
}

//----------------------------------------------------------------------
void vtkSlicerPlaneRepresentation2D::GetActors(vtkPropCollection *pc)
{
  this->PlaneActor->GetActors(pc);
  this->Superclass::GetActors(pc);
}

//----------------------------------------------------------------------
void vtkSlicerPlaneRepresentation2D::ReleaseGraphicsResources(
  vtkWindow *win)
{
  this->PlaneActor->ReleaseGraphicsResources(win);
  this->Superclass::ReleaseGraphicsResources(win);
}

//----------------------------------------------------------------------
int vtkSlicerPlaneRepresentation2D::RenderOverlay(vtkViewport *viewport)
{
  int count=0;
  if (this->PlaneActor->GetVisibility())
    {
    count +=  this->PlaneActor->RenderOverlay(viewport);
    }
  count += this->Superclass::RenderOverlay(viewport);

  return count;
}

//-----------------------------------------------------------------------------
int vtkSlicerPlaneRepresentation2D::RenderOpaqueGeometry(
  vtkViewport *viewport)
{
  int count=0;
  if (this->PlaneActor->GetVisibility())
    {
    count += this->PlaneActor->RenderOpaqueGeometry(viewport);
    }
  count += this->Superclass::RenderOpaqueGeometry(viewport);

  return count;
}

//-----------------------------------------------------------------------------
int vtkSlicerPlaneRepresentation2D::RenderTranslucentPolygonalGeometry(
  vtkViewport *viewport)
{
  int count=0;
  if (this->PlaneActor->GetVisibility())
    {
    count += this->PlaneActor->RenderTranslucentPolygonalGeometry(viewport);
    }
  count += this->Superclass::RenderTranslucentPolygonalGeometry(viewport);

  return count;
}

//-----------------------------------------------------------------------------
vtkTypeBool vtkSlicerPlaneRepresentation2D::HasTranslucentPolygonalGeometry()
{
  if (this->Superclass::HasTranslucentPolygonalGeometry())
    {
    return true;
    }
  if (this->PlaneActor->GetVisibility() && this->PlaneActor->HasTranslucentPolygonalGeometry())
    {
    return true;
    }
  return false;
}

//----------------------------------------------------------------------
double *vtkSlicerPlaneRepresentation2D::GetBounds()
{
  return nullptr;
}

//-----------------------------------------------------------------------------
void vtkSlicerPlaneRepresentation2D::PrintSelf(ostream& os, vtkIndent indent)
{
  //Superclass typedef defined in vtkTypeMacro() found in vtkSetGet.h
  this->Superclass::PrintSelf(os, indent);

  if (this->PlaneActor)
    {
    os << indent << "Line Actor Visibility: " << this->PlaneActor->GetVisibility() << "\n";
    }
  else
    {
    os << indent << "Line Actor: (none)\n";
    }

  os << indent << "Label Format: ";
  os << this->LabelFormat << "\n";
}

//-----------------------------------------------------------------------------
void vtkSlicerPlaneRepresentation2D::SetMarkupsNode(vtkMRMLMarkupsNode *markupsNode)
{
  if (this->MarkupsNode != markupsNode)
    {
    if (markupsNode)
      {
      this->PlaneSliceDistance->SetInputConnection(markupsNode->GetCurveWorldConnection());
      }
    else
      {
      this->PlaneSliceDistance->SetInputData(vtkNew<vtkPolyData>());
      }
    }
  this->Superclass::SetMarkupsNode(markupsNode);
}
