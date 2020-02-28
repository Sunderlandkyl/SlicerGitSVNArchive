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
 * @class   vtkSlicerPlaneRepresentation2D
 * @brief   Default representation for the line widget
 *
 * This class provides the default concrete representation for the
 * vtkMRMLAbstractWidget. See vtkMRMLAbstractWidget
 * for details.
 * @sa
 * vtkSlicerMarkupsWidgetRepresentation2D vtkMRMLAbstractWidget
*/

#ifndef vtkSlicerPlaneRepresentation2D_h
#define vtkSlicerPlaneRepresentation2D_h

#include "vtkSlicerMarkupsModuleVTKWidgetsExport.h"
#include "vtkSlicerMarkupsWidgetRepresentation2D.h"

class vtkAppendPolyData;
class vtkClipPolyData;
class vtkCompositeDataGeometryFilter;
class vtkDiscretizableColorTransferFunction;
class vtkMRMLInteractionEventData;
class vtkPlaneCutter;
class vtkPlaneSource;
class vtkSampleImplicitFunctionFilter;

class VTK_SLICER_MARKUPS_MODULE_VTKWIDGETS_EXPORT vtkSlicerPlaneRepresentation2D : public vtkSlicerMarkupsWidgetRepresentation2D
{
public:
  /// Instantiate this class.
  static vtkSlicerPlaneRepresentation2D *New();

  /// Standard methods for instances of this class.
  vtkTypeMacro(vtkSlicerPlaneRepresentation2D,vtkSlicerMarkupsWidgetRepresentation2D);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Subclasses of vtkContourCurveRepresentation must implement these methods. These
  /// are the methods that the widget and its representation use to
  /// communicate with each other.
  void UpdateFromMRML(vtkMRMLNode* caller, unsigned long event, void *callData = nullptr) override;

  /// Methods to make this class behave as a vtkProp.
  void GetActors(vtkPropCollection *) override;
  void ReleaseGraphicsResources(vtkWindow *) override;
  int RenderOverlay(vtkViewport *viewport) override;
  int RenderOpaqueGeometry(vtkViewport *viewport) override;
  int RenderTranslucentPolygonalGeometry(vtkViewport *viewport) override;
  vtkTypeBool HasTranslucentPolygonalGeometry() override;

  /// Return the bounds of the representation
  double *GetBounds() override;

  void CanInteract(vtkMRMLInteractionEventData* interactionEventData,
    int &foundComponentType, int &foundComponentIndex, double &closestDistance2) override;

  bool GetTransformationReferencePoint(double referencePointWorld[3]) override;

  void BuildPlane();

protected:
  vtkSlicerPlaneRepresentation2D();
  ~vtkSlicerPlaneRepresentation2D() override;

  void SetMarkupsNode(vtkMRMLMarkupsNode *markupsNode) override;

  vtkNew<vtkPlaneSource> PlaneFilter;
  vtkNew<vtkPlaneCutter> PlaneCutter;
  vtkNew<vtkClipPolyData> PlaneClipper;
  vtkNew<vtkCompositeDataGeometryFilter> PlaneCompositeFilter;
  vtkNew<vtkAppendPolyData> PlaneAppend;
  vtkNew<vtkTransformPolyDataFilter> PlaneWorldToSliceTransformer;
  vtkNew<vtkPolyDataMapper2D> PlaneMapper;
  vtkNew<vtkActor2D> PlaneActor;

  vtkNew<vtkTransformPolyDataFilter> ArrowWorldToSliceTransformer;
  vtkNew<vtkMarkupsGlyphSource2D> ArrowFilter;
  vtkNew<vtkPolyDataMapper2D> ArrowMapper;
  vtkNew<vtkActor2D> ArrowActor;

  vtkNew<vtkDiscretizableColorTransferFunction> ColorMap;
  vtkNew<vtkSampleImplicitFunctionFilter> PlaneSliceDistance;
  std::string LabelFormat;

private:
  vtkSlicerPlaneRepresentation2D(const vtkSlicerPlaneRepresentation2D&) = delete;
  void operator=(const vtkSlicerPlaneRepresentation2D&) = delete;
};

#endif
