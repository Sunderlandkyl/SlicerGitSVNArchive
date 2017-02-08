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

  This file was originally developed by Csaba Pinter, PerkLab, Queen's University
  and was supported through the Applied Cancer Research Unit program of Cancer Care
  Ontario with funds provided by the Ontario Ministry of Health and Long-Term Care

==============================================================================*/

// Segmentations includes
#include "qSlicerSegmentEditorAbstractLabelEffect.h"
#include "qSlicerSegmentEditorAbstractLabelEffect_p.h"

#include "vtkOrientedImageData.h"
#include "vtkOrientedImageDataResample.h"
#include "vtkMRMLSegmentationNode.h"
#include "vtkMRMLSegmentEditorNode.h"
#include "vtkSlicerSegmentationsModuleLogic.h"
#include <vtkFractionalLogicalOperations.h>

// Qt includes
#include <QDebug>
#include <QCheckBox>
#include <QLabel>

// CTK includes
#include "ctkRangeWidget.h"

// VTK includes
#include <vtkMatrix4x4.h>
#include <vtkTransform.h>
#include <vtkImageConstantPad.h>
#include <vtkImageMask.h>
#include <vtkImageThreshold.h>
#include <vtkPolyData.h>
#include <vtkImageMathematics.h>
#include <vtkFieldData.h>
#include <vtkDoubleArray.h>
#include <vtkResampleBinaryLabelmapToFractionalLabelmap.h>
#include <vtkPolyDataToImageStencil.h>
#include <vtkNew.h>
#include <vtkRibbonFilter.h>
#include <vtkImageStencilToImage.h>
#include <vtkSphereSource.h> //TODO
#include <vtkPolyDataNormals.h>
#include <vtkRuledSurfaceFilter.h>
#include <vtkCleanPolyData.h>
#include <vtkLinearExtrusionFilter.h>
#include <vtkLassoStencilSource.h>
#include <vtkFillHolesFilter.h>
#include <vtkPolyDataWriter.h>
#include <vtkContourTriangulator.h>
#include <vtkTriangleFilter.h>
#include <vtkAppendPolyData.h>
#include <vtkPlaneSource.h>
#include <vtkClipPolyData.h>
#include <vtkClipClosedSurface.h>
#include <vtkPlaneCollection.h>

#include <vtkPlane.h>
#include <vtkCutter.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkPolyDataMapper2D.h>
#include <vtkProperty2D.h>
#include <vtkActor2D.h>

// Slicer includes
#include "qMRMLSliceWidget.h"
#include "vtkMRMLSliceLogic.h"
#include "vtkMRMLSliceLayerLogic.h"


// MRML includes
#include "vtkMRMLScalarVolumeNode.h"
#include "vtkMRMLTransformNode.h"
#include "vtkMRMLSliceNode.h"


//-----------------------------------------------------------------------------
// qSlicerSegmentEditorAbstractLabelEffectPrivate methods

//-----------------------------------------------------------------------------
qSlicerSegmentEditorAbstractLabelEffectPrivate::qSlicerSegmentEditorAbstractLabelEffectPrivate(qSlicerSegmentEditorAbstractLabelEffect& object)
  : q_ptr(&object)
{
}

//-----------------------------------------------------------------------------
qSlicerSegmentEditorAbstractLabelEffectPrivate::~qSlicerSegmentEditorAbstractLabelEffectPrivate()
{
}

//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// qSlicerSegmentEditorAbstractLabelEffect methods

//----------------------------------------------------------------------------
qSlicerSegmentEditorAbstractLabelEffect::qSlicerSegmentEditorAbstractLabelEffect(QObject* parent)
 : Superclass(parent)
 , d_ptr( new qSlicerSegmentEditorAbstractLabelEffectPrivate(*this) )
{
}

//----------------------------------------------------------------------------
qSlicerSegmentEditorAbstractLabelEffect::~qSlicerSegmentEditorAbstractLabelEffect()
{
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAbstractLabelEffect::referenceGeometryChanged()
{
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAbstractLabelEffect::masterVolumeNodeChanged()
{
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAbstractLabelEffect::setupOptionsFrame()
{
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAbstractLabelEffect::setMRMLDefaults()
{
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAbstractLabelEffect::updateGUIFromMRML()
{
  if (!this->active())
    {
    // updateGUIFromMRML is called when the effect is activated
    return;
    }
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAbstractLabelEffect::updateMRMLFromGUI()
{
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAbstractLabelEffect::appendPolyMask(vtkOrientedImageData* input, vtkPolyData* polyData, qMRMLSliceWidget* sliceWidget)
{
  // Rasterize a poly data onto the input image into the slice view
  // - Points are specified in current XY space
  Q_D(qSlicerSegmentEditorAbstractLabelEffect);

  if (!input)
    {
    //TODO: Error
    return;
    }

  if (!polyData)
    {
    //TODO: Error
    return;
    }

  if (polyData->GetNumberOfPolys() < 1)
    {
    if (polyData->GetNumberOfLines() < 1)
      {
      //TODO: Error
      return;
      }
    d->createMaskImageFromContour(polyData, input, sliceWidget);
    }
  else
    {
    d->createMaskImageFromPolyData(polyData, input);
    }
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAbstractLabelEffectPrivate::createMaskImageFromContour(vtkPolyData* contourPolyData, vtkOrientedImageData* outputMask, qMRMLSliceWidget* sliceWidget)
{

  Q_Q(qSlicerSegmentEditorAbstractLabelEffect);

  if (!outputMask)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid segmentationNode";
    return;
    }

  if (!q->parameterSetNode())
    {
    qCritical() << Q_FUNC_INFO << ": Invalid segment editor parameter set node!";
    return;
    }

  vtkMRMLSegmentationNode* segmentationNode = q->parameterSetNode()->GetSegmentationNode();
  if (!segmentationNode)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid segmentationNode";
    return;
    }

  //TODO: more checks

  vtkNew<vtkMatrix4x4> imageToWorldMatrix;
  outputMask->GetImageToWorldMatrix(imageToWorldMatrix.GetPointer());

  vtkNew<vtkMatrix4x4> inverseImageToWorldMatrix;
  inverseImageToWorldMatrix->DeepCopy(imageToWorldMatrix.GetPointer());
  inverseImageToWorldMatrix->Invert();

  vtkCellArray* lines = contourPolyData->GetLines();

  vtkNew<vtkIdList> line;
  lines->GetCell(0, line.GetPointer());

  vtkNew<vtkIdList> lowerLine;
  vtkNew<vtkIdList> upperLine;

  if (line->GetId(0) != line->GetId(line->GetNumberOfIds()-1))
    {
    line->InsertNextId(line->GetId(0));
    lines->Initialize();
    }
  lines->InsertNextCell(line.GetPointer());

  vtkPoints* linePoints = contourPolyData->GetPoints();

  for (int i=0; i < line->GetNumberOfIds(); ++i)
    {
    double currentPoint[3] = {0,0,0};
    linePoints->GetPoint(line->GetId(i), currentPoint);

    currentPoint[3] = 0.5;
    upperLine->InsertNextId(linePoints->InsertNextPoint(currentPoint));
    lowerLine->DeepCopy(line.GetPointer());

    }

  contourPolyData->SetLines(lines);
  contourPolyData->SetPoints(linePoints);

  vtkNew<vtkPolygon> lowerPolygon;
  lowerP

  bool masterRepresentationIsFractionalLabelmap = segmentationNode->GetSegmentation()->GetMasterRepresentationName() == vtkSegmentationConverter::GetSegmentationFractionalLabelmapRepresentationName();

  vtkNew<vtkCleanPolyData> cleanPolyData;
  cleanPolyData->SetInputData(contourPolyData);
  cleanPolyData->Update();

  double* xyBounds = cleanPolyData->GetOutput()->GetBounds();

  vtkNew<vtkLinearExtrusionFilter> linearExtrusionFilter;
  linearExtrusionFilter->SetInputConnection(cleanPolyData->GetOutputPort());
  linearExtrusionFilter->SetScaleFactor(1.0);
  linearExtrusionFilter->SetExtrusionTypeToVectorExtrusion();
  linearExtrusionFilter->SetVector(0.0, 0.0, 1.0);

  //vtkNew<vtkRibbonFilter> ribbonFilter;
  //ribbonFilter->SetInputConnection(cleanPolyData->GetOutputPort());
  //ribbonFilter->SetWidth(0.5);
  //ribbonFilter->SetAngle(90.0);
  //ribbonFilter->SetDefaultNormal(0,0,1);
  //ribbonFilter->UseDefaultNormalOn();

  vtkNew<vtkFillHolesFilter> fillHolesFilter;
  fillHolesFilter->SetInputConnection(linearExtrusionFilter->GetOutputPort());
  fillHolesFilter->SetHoleSize(VTK_DOUBLE_MAX);

  //vtkNew<vtkPlaneSource> upperPlane;
  //upperPlane->SetOrigin(xyBounds[0], xyBounds[2], xyBounds[4] + 1.0);
  //upperPlane->SetPoint1(xyBounds[0], xyBounds[3], xyBounds[4] + 1.0);
  //upperPlane->SetPoint2(xyBounds[1], xyBounds[2], xyBounds[4] + 1.0);
  //upperPlane->SetNormal(0.0, 0.0, -1.0);
  //upperPlane->Update();

  //vtkNew<vtkPlaneSource> lowerPlane;
  //lowerPlane->SetOrigin(xyBounds[0], xyBounds[2], xyBounds[4]);
  //lowerPlane->SetPoint1(xyBounds[0], xyBounds[3], xyBounds[4]);
  //lowerPlane->SetPoint2(xyBounds[1], xyBounds[2], xyBounds[4]);
  //lowerPlane->SetNormal(0.0, 0.0, 1.0);
  //lowerPlane->Update();

  //vtkNew<vtkAppendPolyData> appendPolydata;
  //appendPolydata->SetInputConnection(linearExtrusionFilter->GetOutputPort());
  //appendPolydata->AddInputDataObject(upperPlane->GetOutput());
  //appendPolydata->AddInputDataObject(lowerPlane->GetOutput());

  //vtkNew<vtkPlane> upperPlane;
  //upperPlane->SetOrigin(xyBounds[0], xyBounds[2], xyBounds[4]+1.0);
  //upperPlane->SetNormal(0, 0, -1);

  //vtkNew<vtkPlane> lowerPlane;
  //lowerPlane->SetOrigin(xyBounds[0], xyBounds[2], xyBounds[4]);
  //lowerPlane->SetNormal(0, 0, 1);

  //vtkNew<vtkPlaneCollection> planeCollection;
  //planeCollection->AddItem(upperPlane.GetPointer());
  //planeCollection->AddItem(lowerPlane.GetPointer());

  //vtkNew<vtkClipClosedSurface> clipClosedSurface;
  //clipClosedSurface->SetInputConnection(linearExtrusionFilter->GetOutputPort());
  //clipClosedSurface->SetClippingPlanes(planeCollection.GetPointer());

  vtkMatrix4x4* sliceXyToRas = sliceWidget->sliceLogic()->GetSliceNode()->GetXYToRAS();
  vtkNew<vtkTransform> sliceXyToIJKTransform;
  sliceXyToIJKTransform->PostMultiply();
  sliceXyToIJKTransform->Identity();
  sliceXyToIJKTransform->Translate(0.0,0.0,-0.5);
  sliceXyToIJKTransform->Concatenate(sliceXyToRas);
  sliceXyToIJKTransform->Concatenate(inverseImageToWorldMatrix.GetPointer());

  vtkNew<vtkTransformPolyDataFilter> transformPolyDataFilter;
  transformPolyDataFilter->SetInputConnection(fillHolesFilter->GetOutputPort());
  //transformPolyDataFilter->SetInputData(polyData);
  transformPolyDataFilter->SetTransform(sliceXyToIJKTransform.GetPointer());
  transformPolyDataFilter->Update();

  //double xyNormal[4] = {0,0,1,0};
  //double ijkNormal[4] = {0,0,0,0};
  //sliceXyToIJKTransform->MultiplyPoint(xyNormal, ijkNormal);
  //ribbonFilter->SetDefaultNormal(ijkNormal);
  //ribbonFilter->UseDefaultNormalOn();

  vtkNew<vtkPolyDataNormals> normalFilter;
  normalFilter->SetInputConnection(transformPolyDataFilter->GetOutputPort());
  normalFilter->ConsistencyOn();
  normalFilter->Update();

  vtkNew<vtkPolyDataWriter> writer;
  writer->SetInputConnection(normalFilter->GetOutputPort());
  writer->SetFileName("E:\\test\\abc.vtk");
  writer->Update();

  this->createMaskImageFromPolyData(normalFilter->GetOutput(), outputMask);

}


//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAbstractLabelEffectPrivate::createMaskImageFromPolyData(vtkPolyData* polyData, vtkOrientedImageData* outputMask)
{
  // Rasterize a poly data onto the input image into the slice view
  // - Points are specified in current XY space

  Q_Q(qSlicerSegmentEditorAbstractLabelEffect);

  vtkSmartPointer<vtkOrientedImageData> modifierLabelmap = vtkSmartPointer<vtkOrientedImageData>::New();
  modifierLabelmap->DeepCopy(q->defaultModifierLabelmap());

  if (!modifierLabelmap)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid segmentationNode";
    return;
    }

  if (!q->parameterSetNode())
    {
    qCritical() << Q_FUNC_INFO << ": Invalid segment editor parameter set node!";
    return;
    }

  vtkMRMLSegmentationNode* segmentationNode = q->parameterSetNode()->GetSegmentationNode();
  if (!segmentationNode)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid segmentationNode";
    return;
    }

  q->saveStateForUndo();

  vtkNew<vtkMatrix4x4> imageToWorldMatrix;
  outputMask->GetImageToWorldMatrix(imageToWorldMatrix.GetPointer());

  vtkNew<vtkMatrix4x4> inverseImageToWorldMatrix;
  inverseImageToWorldMatrix->DeepCopy(imageToWorldMatrix.GetPointer());
  inverseImageToWorldMatrix->Invert();

  bool masterRepresentationIsFractionalLabelmap = false;


  double* boundsRas = polyData->GetBounds();

  int dimensions[3] = { ceil(boundsRas[1]) - floor(boundsRas[0]) + 2,
                        ceil(boundsRas[3]) - floor(boundsRas[2]) + 2,
                        ceil(boundsRas[5]) - floor(boundsRas[4]) + 2 };

  vtkNew<vtkPolyDataToImageStencil> polyDataToStencil;
  polyDataToStencil->SetInputData(polyData);

  if (masterRepresentationIsFractionalLabelmap)
    {
    double offset = 5.0/12.0; //TODO
    polyDataToStencil->SetOutputWholeExtent(0, dimensions[0]*6-1, 0, dimensions[1]*6-1, 0, dimensions[2]*6-1);
    polyDataToStencil->SetOutputOrigin(floor(boundsRas[0])-1 - offset, floor(boundsRas[2])-1 - offset, floor(boundsRas[4])-1 - offset);
    polyDataToStencil->SetOutputSpacing(1.0/6.0, 1.0/6.0, 1.0/6.0);
    }
  else
    {
    polyDataToStencil->SetOutputWholeExtent(0, dimensions[0]-1, 0, dimensions[1]-1, 0, dimensions[2]-1);
    polyDataToStencil->SetOutputOrigin(floor(boundsRas[0])-1, floor(boundsRas[2])-1, floor(boundsRas[4])-1);
    polyDataToStencil->SetOutputSpacing(1.0, 1.0, 1.0);
    }

  vtkNew<vtkImageStencilToImage> stencilToImage;
  stencilToImage->SetInputConnection(polyDataToStencil->GetOutputPort());
  stencilToImage->SetInsideValue(q->m_FillValue);
  stencilToImage->SetOutsideValue(q->m_EraseValue);
  stencilToImage->SetOutputScalarType(modifierLabelmap->GetScalarType());
  stencilToImage->Update();

  if (masterRepresentationIsFractionalLabelmap)
    {
    vtkNew<vtkResampleBinaryLabelmapToFractionalLabelmap> resampleBinaryLabelmapToFractionalLabelmap;
    resampleBinaryLabelmapToFractionalLabelmap->SetInputConnection(stencilToImage->GetOutputPort());
    resampleBinaryLabelmapToFractionalLabelmap->SetOutputMinimumValue(-108);
    resampleBinaryLabelmapToFractionalLabelmap->SetOutputScalarType(VTK_CHAR);
    int outputExtent[6] = {0, dimensions[0]-1, 0, dimensions[1]-1, 0, dimensions[2]-1};
    resampleBinaryLabelmapToFractionalLabelmap->SetOutputExtent(outputExtent);
    resampleBinaryLabelmapToFractionalLabelmap->Update();
    outputMask->DeepCopy(resampleBinaryLabelmapToFractionalLabelmap->GetOutput());
    }
  else
    {
    outputMask->DeepCopy(stencilToImage->GetOutput());
    }

  outputMask->DeepCopy(stencilToImage->GetOutput());
  outputMask->SetImageToWorldMatrix(imageToWorldMatrix.GetPointer());
  outputMask->SetOrigin(imageToWorldMatrix->MultiplyDoublePoint(polyDataToStencil->GetOutputOrigin()));

  vtkFractionalLogicalOperations::Write(outputMask, "E:\\test\\polyMask.nrrd");

  return;
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAbstractLabelEffect::imageToWorldMatrix(vtkMRMLVolumeNode* node, vtkMatrix4x4* ijkToRas)
{
  if (!node || !ijkToRas)
    {
    return;
    }

  node->GetIJKToRASMatrix(ijkToRas);

  vtkMRMLTransformNode* transformNode = node->GetParentTransformNode();
  if (transformNode)
    {
    if (transformNode->IsTransformToWorldLinear())
      {
      vtkSmartPointer<vtkMatrix4x4> volumeRasToWorldRas = vtkSmartPointer<vtkMatrix4x4>::New();
      transformNode->GetMatrixTransformToWorld(volumeRasToWorldRas);
      vtkMatrix4x4::Multiply4x4(volumeRasToWorldRas, ijkToRas, ijkToRas);
      }
    else
      {
      qCritical() << Q_FUNC_INFO << ": Parent transform is non-linear, which cannot be handled! Skipping.";
      }
    }
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAbstractLabelEffect::imageToWorldMatrix(vtkOrientedImageData* image, vtkMRMLSegmentationNode* node, vtkMatrix4x4* ijkToRas)
{
  if (!image || !node || !ijkToRas)
    {
    return;
    }

  image->GetImageToWorldMatrix(ijkToRas);

  vtkMRMLTransformNode* transformNode = node->GetParentTransformNode();
  if (transformNode)
    {
    if (transformNode->IsTransformToWorldLinear())
      {
      vtkSmartPointer<vtkMatrix4x4> segmentationRasToWorldRas = vtkSmartPointer<vtkMatrix4x4>::New();
      transformNode->GetMatrixTransformToWorld(segmentationRasToWorldRas);
      vtkMatrix4x4::Multiply4x4(segmentationRasToWorldRas, ijkToRas, ijkToRas);
      }
    else
      {
      qCritical() << Q_FUNC_INFO << ": Parent transform is non-linear, which cannot be handled! Skipping.";
      }
    }
}
