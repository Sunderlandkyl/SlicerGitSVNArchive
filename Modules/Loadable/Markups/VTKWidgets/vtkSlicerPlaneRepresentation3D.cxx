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
#include <vtkActor2D.h>
#include <vtkArcSource.h>
#include <vtkArrowSource.h>
#include <vtkCellLocator.h>
#include <vtkDoubleArray.h>
#include <vtkGlyph3DMapper.h>
#include <vtkPlaneSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkPointData.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkSlicerPlaneRepresentation3D.h>
#include <vtkTextActor.h>
#include <vtkTextProperty.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>

// MRML includes
#include "vtkMRMLInteractionEventData.h"
#include "vtkMRMLMarkupsDisplayNode.h"
#include "vtkMRMLMarkupsPlaneNode.h"

vtkStandardNewMacro(vtkSlicerPlaneRepresentation3D);

//----------------------------------------------------------------------
vtkSlicerPlaneRepresentation3D::vtkSlicerPlaneRepresentation3D()
{
  this->PlaneFilter = vtkSmartPointer<vtkPlaneSource>::New();

  this->PlaneMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  this->PlaneMapper->SetInputData(vtkNew<vtkPolyData>());

  this->PlaneActor = vtkSmartPointer<vtkActor>::New();
  this->PlaneActor->SetMapper(this->PlaneMapper);
  this->PlaneActor->PickableOff(); // TODO: Test. We may want disable/renable as the plane is being interacted with to prevent it from picking itself.
  this->PlaneActor->SetProperty(this->GetControlPointsPipeline(Unselected)->Property);

  this->ArrowFilter = vtkSmartPointer<vtkArrowSource>::New();
  this->ArrowFilter->SetTipResolution(50);

  this->ArrowMapper = vtkSmartPointer<vtkGlyph3DMapper>::New();
  this->ArrowMapper->SetOrientationModeToDirection();
  this->ArrowMapper->SetOrientationArray(vtkDataObject::FIELD_ASSOCIATION_POINTS);
  this->ArrowMapper->SetSourceConnection(this->ArrowFilter->GetOutputPort());
  this->ArrowMapper->SetScalarVisibility(false);

  this->ArrowActor = vtkSmartPointer<vtkActor>::New();
  this->ArrowActor->SetMapper(this->ArrowMapper);
  this->ArrowActor->SetProperty(this->GetControlPointsPipeline(Unselected)->Property);

  this->LabelFormat = "%-#6.3g";
}

//----------------------------------------------------------------------
vtkSlicerPlaneRepresentation3D::~vtkSlicerPlaneRepresentation3D()
= default;

//----------------------------------------------------------------------
bool vtkSlicerPlaneRepresentation3D::GetTransformationReferencePoint(double referencePointWorld[3])
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
void vtkSlicerPlaneRepresentation3D::BuildPlane()
{
  vtkMRMLMarkupsPlaneNode* markupsNode = vtkMRMLMarkupsPlaneNode::SafeDownCast(this->GetMarkupsNode());
  if (!markupsNode || markupsNode->GetNumberOfControlPoints() != 3)
    {
    this->PlaneMapper->SetInputData(vtkNew<vtkPolyData>());
    this->ArrowMapper->SetInputData(vtkNew<vtkPolyData>());
    return;
    }

  double x[3], y[3], z[3] = { 0 };
  markupsNode->GetVectors(x, y, z);

  if (vtkMath::Norm(x) <= 0.0001 || vtkMath::Norm(y) <= 0.0001 || vtkMath::Norm(z) <= 0.0001)
    {
    this->PlaneMapper->SetInputData(vtkNew<vtkPolyData>());
    this->ArrowMapper->SetInputData(vtkNew<vtkPolyData>());
    return;
    }

  this->PlaneMapper->SetInputConnection(this->PlaneFilter->GetOutputPort());

  double origin[3] = { 0.0 };
  double point1[3] = { 0.0 };
  double point2[3] = { 0.0 };
  markupsNode->GetNthControlPointPositionWorld(0, origin);
  markupsNode->GetNthControlPointPositionWorld(1, point1);
  markupsNode->GetNthControlPointPositionWorld(2, point2);

  // Update the normal vector
  vtkNew<vtkPoints> points;
  points->InsertNextPoint(origin);

  vtkNew<vtkDoubleArray> directionArray;
  directionArray->SetNumberOfComponents(3);
  directionArray->InsertNextTuple3(z[0], z[1], z[2]);
  directionArray->SetName("direction");

  vtkNew<vtkPolyData> polyData;
  polyData->SetPoints(points);
  polyData->GetPointData()->SetScalars(directionArray);

  this->ArrowMapper->SetInputData(polyData);
  this->ArrowMapper->SetScaleFactor(this->ControlPointSize * 3);
  this->ArrowMapper->Update();

  // Update the plane
  double vector1[3] = { 0 };
  vtkMath::Subtract(point1, origin, vector1);

  double vector2[3] = { 0 };
  vtkMath::Subtract(point2, origin, vector2);

  double point1X = std::abs(vtkMath::Dot(vector1, x));
  double point2X = std::abs(vtkMath::Dot(vector2, x));
  double xMax = std::max({ 0.0, point1X, point2X });
  vtkMath::MultiplyScalar(x, xMax);

  double point1Y = std::abs(vtkMath::Dot(vector1, y));
  double point2Y = std::abs(vtkMath::Dot(vector2, y));
  double yMax = std::max({ 0.0, point1Y, point2Y });
  vtkMath::MultiplyScalar(y, yMax);

  double planePoint1[3] = { 0 };
  vtkMath::Subtract(origin, x, planePoint1);
  vtkMath::Subtract(planePoint1, y, planePoint1);

  double planePoint2[3] = { 0 };
  vtkMath::Subtract(origin, x, planePoint2);
  vtkMath::Add(planePoint2, y, planePoint2);

  double planePoint3[3] = { 0 };
  vtkMath::Add(origin, x, planePoint3);
  vtkMath::Subtract(planePoint3, y, planePoint3);

  vtkMath::Normalize(x);
  vtkMath::Normalize(y);

  this->PlaneFilter->SetOrigin(planePoint1);
  this->PlaneFilter->SetPoint1(planePoint2);
  this->PlaneFilter->SetPoint2(planePoint3);
}

//----------------------------------------------------------------------
void vtkSlicerPlaneRepresentation3D::UpdateFromMRML(vtkMRMLNode* caller, unsigned long event, void *callData /*=nullptr*/)
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

  // Update plane geometry
  this->BuildPlane();

  // Update lines display properties

  this->TextActor->SetVisibility(this->MarkupsDisplayNode->GetPropertiesLabelVisibility());

  this->UpdateRelativeCoincidentTopologyOffsets(this->PlaneMapper);

  bool lineVisibility = this->GetAllControlPointsVisible();
  this->PlaneActor->SetVisibility(lineVisibility);
  this->ArrowActor->SetVisibility(lineVisibility);
  this->TextActor->SetVisibility(lineVisibility && markupsNode->GetNumberOfControlPoints() == 3);

  int controlPointType = Active;
  if (this->MarkupsDisplayNode->GetActiveComponentType() != vtkMRMLMarkupsDisplayNode::ComponentLine)
    {
    controlPointType = this->GetAllControlPointsSelected() ? Selected : Unselected;
    }
  this->PlaneActor->SetProperty(this->GetControlPointsPipeline(controlPointType)->Property);
  this->ArrowActor->SetProperty(this->GetControlPointsPipeline(controlPointType)->Property);
  this->TextActor->SetTextProperty(this->GetControlPointsPipeline(controlPointType)->TextProperty);
}

//----------------------------------------------------------------------
void vtkSlicerPlaneRepresentation3D::GetActors(vtkPropCollection *pc)
{
  this->Superclass::GetActors(pc);
  this->PlaneActor->GetActors(pc);
  this->ArrowActor->GetActors(pc);
  this->TextActor->GetActors(pc);
}

//----------------------------------------------------------------------
void vtkSlicerPlaneRepresentation3D::ReleaseGraphicsResources(
  vtkWindow *win)
{
  this->Superclass::ReleaseGraphicsResources(win);
  this->PlaneActor->ReleaseGraphicsResources(win);
  this->ArrowActor->ReleaseGraphicsResources(win);
  this->TextActor->ReleaseGraphicsResources(win);
}

//----------------------------------------------------------------------
int vtkSlicerPlaneRepresentation3D::RenderOverlay(vtkViewport *viewport)
{
  int count=0;
  count = this->Superclass::RenderOverlay(viewport);
  if (this->PlaneActor->GetVisibility())
    {
    count += this->PlaneActor->RenderOverlay(viewport);
    }
  if (this->ArrowActor->GetVisibility())
    {
    count += this->ArrowActor->RenderOverlay(viewport);
    }
  if (this->TextActor->GetVisibility())
    {
    count += this->TextActor->RenderOverlay(viewport);
    }
  return count;
}

//-----------------------------------------------------------------------------
int vtkSlicerPlaneRepresentation3D::RenderOpaqueGeometry(
  vtkViewport *viewport)
{
  int count=0;
  count = this->Superclass::RenderOpaqueGeometry(viewport);
  if (this->PlaneActor->GetVisibility())
    {
    count += this->PlaneActor->RenderOpaqueGeometry(viewport);
    }
  if (this->ArrowActor->GetVisibility())
    {
    this->ArrowMapper->SetScaleFactor(this->ControlPointSize * 3);
    this->ArrowMapper->Update();
    count += this->ArrowActor->RenderOpaqueGeometry(viewport);
    }
  if (this->TextActor->GetVisibility())
    {
    count += this->TextActor->RenderOpaqueGeometry(viewport);
    }
  return count;
}

//-----------------------------------------------------------------------------
int vtkSlicerPlaneRepresentation3D::RenderTranslucentPolygonalGeometry(
  vtkViewport *viewport)
{
  int count=0;
  count = this->Superclass::RenderTranslucentPolygonalGeometry(viewport);
  if (this->PlaneActor->GetVisibility())
    {
    count += this->PlaneActor->RenderTranslucentPolygonalGeometry(viewport);
    }
  if (this->ArrowActor->GetVisibility())
    {
    count += this->ArrowActor->RenderTranslucentPolygonalGeometry(viewport);
    }
  if (this->TextActor->GetVisibility())
    {
    count += this->TextActor->RenderTranslucentPolygonalGeometry(viewport);
    }
  return count;
}

//-----------------------------------------------------------------------------
vtkTypeBool vtkSlicerPlaneRepresentation3D::HasTranslucentPolygonalGeometry()
{
  if (this->Superclass::HasTranslucentPolygonalGeometry())
    {
    return true;
    }
  if (this->PlaneActor->GetVisibility() && this->PlaneActor->HasTranslucentPolygonalGeometry())
    {
    return true;
    }
  if (this->ArrowActor->GetVisibility() && this->ArrowActor->HasTranslucentPolygonalGeometry())
    {
    return true;
    }
  if (this->TextActor->GetVisibility() && this->TextActor->HasTranslucentPolygonalGeometry())
    {
    return true;
    }
  return false;
}

//----------------------------------------------------------------------
double *vtkSlicerPlaneRepresentation3D::GetBounds()
{
  vtkBoundingBox boundingBox;
  const std::vector<vtkProp*> actors({ this->PlaneActor });
  this->AddActorsBounds(boundingBox, actors, Superclass::GetBounds());
  boundingBox.GetBounds(this->Bounds);
  return this->Bounds;
}

//----------------------------------------------------------------------
void vtkSlicerPlaneRepresentation3D::CanInteract(
  vtkMRMLInteractionEventData* interactionEventData,
  int &foundComponentType, int &foundComponentIndex, double &closestDistance2)
{
  foundComponentType = vtkMRMLMarkupsDisplayNode::ComponentNone;
  vtkMRMLMarkupsNode* markupsNode = this->GetMarkupsNode();
  if ( !markupsNode || markupsNode->GetLocked() || markupsNode->GetNumberOfControlPoints() < 1
    || !interactionEventData )
    {
    return;
    }
  Superclass::CanInteract(interactionEventData, foundComponentType, foundComponentIndex, closestDistance2);
  if (foundComponentType != vtkMRMLMarkupsDisplayNode::ComponentNone)
    {
    return;
    }

  this->CanInteractWithPlane(interactionEventData, foundComponentType, foundComponentIndex, closestDistance2);
}

//-----------------------------------------------------------------------------
void vtkSlicerPlaneRepresentation3D::CanInteractWithPlane(
  vtkMRMLInteractionEventData* interactionEventData,
  int& foundComponentType, int& foundComponentIndex, double& closestDistance2)
{
  // Create the tree
  vtkSmartPointer<vtkCellLocator> cellLocator =
    vtkSmartPointer<vtkCellLocator>::New();
  this->PlaneFilter->Update();
  cellLocator->SetDataSet(this->PlaneFilter->GetOutput());
  cellLocator->BuildLocator();

  const double* worldPosition = interactionEventData->GetWorldPosition();
  double closestPoint[3];//the coordinates of the closest point will be returned here
  double distance2; //the squared distance to the closest point will be returned here
  vtkIdType cellId; //the cell id of the cell containing the closest point will be returned here
  int subId; //this is rarely used (in triangle strips only, I believe)
  cellLocator->FindClosestPoint(worldPosition, closestPoint, cellId, subId, distance2);

  double toleranceWorld = this->ControlPointSize * this->ControlPointSize;
  if (distance2 < toleranceWorld)
    {
    closestDistance2 = distance2;
    foundComponentType = vtkMRMLMarkupsDisplayNode::ComponentLine; // TODO: Component plane?
    foundComponentIndex = 0;
    }
}

//-----------------------------------------------------------------------------
void vtkSlicerPlaneRepresentation3D::PrintSelf(ostream& os, vtkIndent indent)
{
  //Superclass typedef defined in vtkTypeMacro() found in vtkSetGet.h
  this->Superclass::PrintSelf(os, indent);

  if (this->PlaneActor)
    {
    os << indent << "Plane Visibility: " << this->PlaneActor->GetVisibility() << "\n";
    }
  else
    {
    os << indent << "Plane Visibility: (none)\n";
    }

  if (this->ArrowActor)
    {
    os << indent << "Arrow Visibility: " << this->ArrowActor->GetVisibility() << "\n";
    }
  else
    {
    os << indent << "Plane Visibility: (none)\n";
    }

  if (this->TextActor)
    {
    os << indent << "Text Visibility: " << this->TextActor->GetVisibility() << "\n";
    }
  else
    {
    os << indent << "Text Visibility: (none)\n";
    }

  os << indent << "Label Format: ";
  os << this->LabelFormat << "\n";
}
