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

/**
 * @class   vtkSlicerPlaneRepresentation3D
 * @brief   Default representation for the plane widget
 *
 * This class provides the default concrete representation for the
 * vtkMRMLAbstractWidget. See vtkMRMLAbstractWidget
 * for details.
 * @sa
 * vtkSlicerMarkupsWidgetRepresentation3D vtkMRMLAbstractWidget
*/

#ifndef vtkSlicerPlaneRepresentation3D_h
#define vtkSlicerPlaneRepresentation3D_h

#include "vtkSlicerMarkupsModuleVTKWidgetsExport.h"
#include "vtkSlicerMarkupsWidgetRepresentation3D.h"

class vtkActor;
class vtkArrowSource;
class vtkGlyph3DMapper;
class vtkMRMLInteractionEventData;
class vtkPlaneSource;
class vtkPolyDataMapper;
class vtkPolyData;
class vtkTextActor;
class vtkTransformPolyDataFilter;

class VTK_SLICER_MARKUPS_MODULE_VTKWIDGETS_EXPORT vtkSlicerPlaneRepresentation3D : public vtkSlicerMarkupsWidgetRepresentation3D
{
public:
  /// Instantiate this class.
  static vtkSlicerPlaneRepresentation3D *New();

  /// Standard methods for instances of this class.
  vtkTypeMacro(vtkSlicerPlaneRepresentation3D,vtkSlicerMarkupsWidgetRepresentation3D);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Subclasses of vtkMRMLAbstractWidgetRepresentation must implement these methods. These
  /// are the methods that the widget and its representation use to
  /// communicate with each other.
  void UpdateFromMRML(vtkMRMLNode* caller, unsigned long event, void *callData=nullptr) override;

  /// Methods to make this class behave as a vtkProp.
  void GetActors(vtkPropCollection *) override;
  void ReleaseGraphicsResources(vtkWindow *) override;
  int RenderOverlay(vtkViewport *viewport) override;
  int RenderOpaqueGeometry(vtkViewport *viewport) override;
  int RenderTranslucentPolygonalGeometry(vtkViewport *viewport) override;
  vtkTypeBool HasTranslucentPolygonalGeometry() override;

  /// Return the bounds of the representation
  double *GetBounds() override;

  bool GetTransformationReferencePoint(double referencePointWorld[3]) override;

  void CanInteract(vtkMRMLInteractionEventData* interactionEventData,
    int &foundComponentType, int &foundComponentIndex, double &closestDistance2) override;

  void CanInteractWithPlane(vtkMRMLInteractionEventData* interactionEventData,
    int& foundComponentType, int& foundComponentIndex, double& closestDistance2);

protected:
  vtkSlicerPlaneRepresentation3D();
  ~vtkSlicerPlaneRepresentation3D() override;

  vtkSmartPointer<vtkPlaneSource>    PlaneFilter;
  vtkSmartPointer<vtkPolyDataMapper> PlaneMapper;
  vtkSmartPointer<vtkActor>          PlaneActor;

  vtkSmartPointer<vtkArrowSource>    ArrowFilter;
  vtkSmartPointer<vtkGlyph3DMapper>  ArrowMapper;
  vtkSmartPointer<vtkActor>          ArrowActor;

  std::string LabelFormat;

  void BuildPlane();

private:
  vtkSlicerPlaneRepresentation3D(const vtkSlicerPlaneRepresentation3D&) = delete;
  void operator=(const vtkSlicerPlaneRepresentation3D&) = delete;
};

#endif
