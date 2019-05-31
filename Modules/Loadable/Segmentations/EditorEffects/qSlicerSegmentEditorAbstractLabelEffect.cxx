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

#include "vtkFractionalOperations.h"
#include "vtkOrientedImageData.h"
#include "vtkOrientedImageDataResample.h"
#include "vtkMRMLSegmentationNode.h"
#include "vtkMRMLSegmentEditorNode.h"
#include "vtkSlicerSegmentationsModuleLogic.h"
#include "vtkResampleBinaryLabelmapToFractionalLabelmap.h"

// Qt includes
#include <QDebug>
#include <QCheckBox>
#include <QLabel>

// CTK includes
#include "ctkRangeWidget.h"

// VTK includes
#include <vtkDoubleArray.h>
#include <vtkFieldData.h>
#include <vtkImageConstantPad.h>
#include <vtkImageMask.h>
#include <vtkImageMathematics.h>
#include <vtkMatrix4x4.h>
#include <vtkImageThreshold.h>
#include <vtkTransform.h>
#include <vtkPolyData.h>

// Slicer includes
#include "qMRMLSliceWidget.h"
#include "vtkImageFillROI.h"

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
= default;

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
= default;

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
void qSlicerSegmentEditorAbstractLabelEffect::appendPolyMask(vtkOrientedImageData* input, vtkPolyData* polyData,
                                                             qMRMLSliceWidget* sliceWidget,bool isFractional/*=false*/)
{
  // Rasterize a poly data onto the input image into the slice view
  // - Points are specified in current XY space
  vtkSmartPointer<vtkOrientedImageData> polyMaskImage = vtkSmartPointer<vtkOrientedImageData>::New();

  double bounds[6] = {0,0,0,0,0,0};

  if (isFractional)
    {
    vtkFractionalOperations::CopyFractionalParameters(polyMaskImage, input);
    polyMaskImage->AllocateScalars(input->GetScalarType(),1); //TODO: change scalar type?
    }

  double scalarRange[2] = {0.0, 1.0};
  if (isFractional)
    {
    vtkDoubleArray* scalarRangeArray = vtkDoubleArray::SafeDownCast(polyMaskImage->GetFieldData()->GetAbstractArray(
      vtkSegmentationConverter::GetScalarRangeFieldName()));
    if (scalarRangeArray && scalarRangeArray->GetNumberOfValues() == 2)
      {
      scalarRange[0] = scalarRangeArray->GetValue(0);
      scalarRange[1] = scalarRangeArray->GetValue(1);
      }
    }

  qSlicerSegmentEditorAbstractLabelEffect::createMaskImageFromPolyData(polyData, polyMaskImage, sliceWidget, isFractional);

  vtkFractionalOperations::CopyFractionalParameters(polyMaskImage, input);

  vtkSmartPointer<vtkOrientedImageData> resampledImage = vtkSmartPointer<vtkOrientedImageData>::New();
  vtkOrientedImageDataResample::ResampleOrientedImageToReferenceOrientedImage(polyMaskImage, input, resampledImage, isFractional, false, NULL, scalarRange[0]);

  input->DeepCopy(resampledImage);
  vtkFractionalOperations::CopyFractionalParameters(input, polyMaskImage);

}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAbstractLabelEffect::appendImage(vtkOrientedImageData* inputImage, vtkOrientedImageData* appendedImage, bool isFractional/*=false*/)
{
  if (!inputImage || !appendedImage)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid inputs!";
    return;
    }

  double scalarRange[2] = {0.0, 1.0};
  if (isFractional)
    {
    vtkFractionalOperations::GetScalarRange(inputImage, scalarRange);
    }

  // Make sure appended image has the same lattice as the input image
  vtkSmartPointer<vtkOrientedImageData> resampledAppendedImage = vtkSmartPointer<vtkOrientedImageData>::New();
  vtkOrientedImageDataResample::ResampleOrientedImageToReferenceOrientedImage(
    appendedImage, inputImage, resampledAppendedImage, isFractional, false, NULL, scalarRange[0]);

  // Add image created from poly data to input image
  vtkSmartPointer<vtkImageMathematics> imageMath = vtkSmartPointer<vtkImageMathematics>::New();
  imageMath->SetInput1Data(inputImage);
  imageMath->SetInput2Data(resampledAppendedImage);
  imageMath->SetOperationToMax();
  imageMath->Update();
  inputImage->DeepCopy(imageMath->GetOutput());
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAbstractLabelEffect::createMaskImageFromPolyData(vtkPolyData* polyData, vtkOrientedImageData* outputMask,
                                                                          qMRMLSliceWidget* sliceWidget, bool isFractional/*=false*/)
{
  if (!polyData || !outputMask)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid inputs!";
    return;
    }
  vtkMRMLSliceNode* sliceNode = vtkMRMLSliceNode::SafeDownCast(
    qSlicerSegmentEditorAbstractEffect::viewNode(sliceWidget) );
  if (!sliceNode)
    {
    qCritical() << Q_FUNC_INFO << ": Failed to get slice node!";
    return;
    }

  // Need to know the mapping from RAS into polygon space
  // so the painter can use this as a mask
  // - Need the bounds in RAS space
  // - Need to get an IJKToRAS for just the mask area
  // - Directions are the XYToRAS for this slice
  // - Origin is the lower left of the polygon bounds
  // - TODO: need to account for the boundary pixels
  //
  // Note: uses the slicer2-based vtkImageFillROI filter
  vtkSmartPointer<vtkTransform> xyToSliceTransform = vtkSmartPointer<vtkTransform>::New();
  xyToSliceTransform->SetMatrix(sliceNode->GetXYToSlice());

  vtkSmartPointer<vtkPoints> slicePoints = vtkSmartPointer<vtkPoints>::New();
  xyToSliceTransform->TransformPoints(polyData->GetPoints(), slicePoints);

  polyData->GetPoints()->Modified();
  double bounds[6] = {0,0,0,0,0,0};
  slicePoints->GetBounds(bounds);

  double xlo = bounds[0] - 1.0;
  double xhi = bounds[1];
  double ylo = bounds[2] - 1.0;
  double yhi = bounds[3];

  int oversamplingFactor = 6;

  vtkSmartPointer<vtkTransform> sliceToRASTransform = vtkSmartPointer<vtkTransform>::New();
  sliceToRASTransform->PostMultiply();
  sliceToRASTransform->Identity();
  sliceToRASTransform->Translate(xlo, ylo, 0.0);
  sliceToRASTransform->Concatenate(sliceNode->GetSliceToRAS());

  // Get a good size for the draw buffer
  // - Needs to include the full region of the polygon
  // - Plus a little extra
  //
  // Round to int and add extra pixel for both sides
  // TODO: figure out why we need to add buffer pixels on each
  //   side for the width in order to end up with a single extra
  //   pixel in the rasterized image map.  Probably has to
  //   do with how boundary conditions are handled in the filler
  int w = (int)(xhi - xlo) + 32;
  int h = (int)(yhi - ylo) + 32;

  vtkSmartPointer<vtkOrientedImageData> imageData = vtkSmartPointer<vtkOrientedImageData>::New();
  if (isFractional)
    {
    imageData->SetDimensions(oversamplingFactor*w, oversamplingFactor*h, 1);
    }
  else
    {
    imageData->SetDimensions(w, h, 1);
    }
  imageData->AllocateScalars(VTK_UNSIGNED_CHAR, 1);

  // Move the points so the lower left corner of the bounding box is at 1, 1 (to avoid clipping)
  vtkSmartPointer<vtkTransform> transform = vtkSmartPointer<vtkTransform>::New();
  transform->Identity();
  if (isFractional)
    {
    transform->Scale(oversamplingFactor, oversamplingFactor, 1.0);
    double offset[3] = { (oversamplingFactor-1.0)/(2.0*oversamplingFactor),
                         (oversamplingFactor-1.0)/(2.0*oversamplingFactor),
                          0.0};
    transform->Translate(offset);
    }
  transform->Translate(-xlo, -ylo, 0.0);

  vtkSmartPointer<vtkPoints> drawPoints = vtkSmartPointer<vtkPoints>::New();
  drawPoints->Reset();
  transform->TransformPoints(slicePoints, drawPoints);
  drawPoints->Modified();

  vtkSmartPointer<vtkImageFillROI> fill = vtkSmartPointer<vtkImageFillROI>::New();
  fill->SetInputData(imageData);
  fill->SetValue(1);
  fill->SetPoints(drawPoints);
  fill->Update();

  if (isFractional)
    {
    double scalarRange[2] = {0.0, 1.0};
    vtkFractionalOperations::GetScalarRange(outputMask, scalarRange);

    vtkSmartPointer<vtkOrientedImageData> oversampledBinaryImage = vtkSmartPointer<vtkOrientedImageData>::New();
    oversampledBinaryImage->ShallowCopy(fill->GetOutput());

    vtkSmartPointer<vtkResampleBinaryLabelmapToFractionalLabelmap> fractionalLabelmapFilter =
      vtkSmartPointer<vtkResampleBinaryLabelmapToFractionalLabelmap>::New();
    fractionalLabelmapFilter->SetInputData(oversampledBinaryImage);
    fractionalLabelmapFilter->SetOutputScalarType(outputMask->GetScalarType());
    fractionalLabelmapFilter->SetStepSize(oversamplingFactor);
    fractionalLabelmapFilter->SetOutputMinimumValue(scalarRange[0]);
    fractionalLabelmapFilter->Update();
    outputMask->DeepCopy(fractionalLabelmapFilter->GetOutput());

    }
  else
    {
    outputMask->DeepCopy(fill->GetOutput());
    }

  outputMask->SetGeometryFromImageToWorldMatrix(sliceToRASTransform->GetMatrix());
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
