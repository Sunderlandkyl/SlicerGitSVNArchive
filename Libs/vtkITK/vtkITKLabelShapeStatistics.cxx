/*=========================================================================

  Copyright Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Program:   vtkITK
  Module:    $HeadURL: http://svn.slicer.org/Slicer4/trunk/Libs/vtkITK/vtkITKLabelShapeStatistics.cxx $
  Date:      $Date: 2006-12-21 07:21:52 -0500 (Thu, 21 Dec 2006) $
  Version:   $Revision: 1900 $

==========================================================================*/

#include "vtkITKLabelShapeStatistics.h"

// VTK includes
#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkMatrix4x4.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>

// ITK includes
#include <itkLabelImageToShapeLabelMapFilter.h>
#include <itkShapeLabelObject.h>
#include <itkVTKImageToImageFilter.h>

vtkStandardNewMacro(vtkITKLabelShapeStatistics);

vtkITKLabelShapeStatistics::vtkITKLabelShapeStatistics()
{
  this->ComputeFeretDiameter = false;
  this->ComputeOrientedBoundingBox = false;
  this->ComputePerimeter = false;
  this->Directions = nullptr;
}

vtkITKLabelShapeStatistics::~vtkITKLabelShapeStatistics()
= default;

void vtkITKLabelShapeStatistics::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  os << indent << "ComputeFeretDiameter: " << ComputeFeretDiameter << std::endl;
  os << indent << "ComputeOrientedBoundingBox: " << ComputeOrientedBoundingBox << std::endl;
  os << indent << "ComputePerimeter: " << ComputePerimeter << std::endl;
}

// Note: local function not method - conforms to signature in itkCommand.h
void vtkITKLabelShapeStatisticsHandleProgressEvent (itk::Object *caller,
                                          const itk::EventObject& vtkNotUsed(eventObject),
                                          void *clientdata)
{
  itk::ProcessObject *itkFilter = dynamic_cast<itk::ProcessObject*>(caller);
  vtkAlgorithm *vtkFilter = reinterpret_cast<vtkAlgorithm*>(clientdata);
  if ( itkFilter && vtkFilter )
    {
    vtkFilter->UpdateProgress ( itkFilter->GetProgress() );
    }
};

template <class T>
void vtkITKLabelShapeStatisticsExecute(vtkITKLabelShapeStatistics* self, vtkImageData* input,
  vtkMatrix4x4* directionMatrix, T* vtkNotUsed(inPtr))
{
  // Wrap VTK image into an ITK image
  typedef itk::Image<T, 3> ImageType;
  using VTKToITKFilterType = itk::VTKImageToImageFilter<ImageType>;
  VTKToITKFilterType::Pointer vtkToITKFilter = VTKToITKFilterType::New();
  vtkToITKFilter->SetInput(input);
  vtkToITKFilter->Update();
  ImageType* inImage = vtkToITKFilter->GetOutput();
  typename ImageType::RegionType region;
  typename ImageType::IndexType index;
  typename ImageType::SizeType size;
  using ShapeLabelObjectType = itk::ShapeLabelObject<T, 3>;
  using LabelMapType = itk::LabelMap<ShapeLabelObjectType>;

  // TODO: When vtkImageData is updated to include direction, this should be updated
  if (directionMatrix)
    {
    ImageType::DirectionType gridDirectionMatrix;
    for (unsigned int row = 0; row < 3; row++)
      {
      for (unsigned int column = 0; column < 3; column++)
        {
        gridDirectionMatrix(row, column) = directionMatrix->GetElement(row, column);
        }
      }
    inImage->SetDirection(gridDirectionMatrix);
    }

  // set up the progress callback
  itk::CStyleCommand::Pointer progressCommand = itk::CStyleCommand::New();
  progressCommand->SetClientData(static_cast<void*>(self));
  progressCommand->SetCallback( vtkITKLabelShapeStatisticsHandleProgressEvent );

  typedef itk::LabelImageToShapeLabelMapFilter<ImageType, LabelMapType> LableShapeFilterType;
  typename LableShapeFilterType::Pointer labelFilter = LableShapeFilterType::New();
  labelFilter->AddObserver(itk::ProgressEvent(), progressCommand);
  labelFilter->SetInput(inImage);
  labelFilter->SetComputeFeretDiameter(self->GetComputeFeretDiameter());
  labelFilter->SetComputePerimeter(self->GetComputePerimeter());
  labelFilter->SetComputeOrientedBoundingBox(self->GetComputeOrientedBoundingBox());
  labelFilter->Update();

  LabelMapType::Pointer labelShapeObject = labelFilter->GetOutput();
  const std::vector<typename ShapeLabelObjectType::LabelType> labels = labelShapeObject->GetLabels(); 

  self->ClearCentroids();
  for (int i = 0; i < labels.size(); ++i)
    {
    int label = labels[i];

    ShapeLabelObjectType::ShapeLabelObject::Pointer labelObject = labelShapeObject->GetLabelObject(label);
    if (!labelObject)
      {
      continue;
      }

    ShapeLabelObjectType::CentroidType centroidObject = labelObject->GetCentroid();    
    double centroid[3] = { centroidObject[0], centroidObject[1], centroidObject[2] };
    self->AddCentroid(label, centroid);
    }

  if (self->GetComputeFeretDiameter())
    {
    // TODO
    }

  if (self->GetComputeOrientedBoundingBox())
    {
    // TODO
    }

  if (self->GetComputePerimeter())
    {
    // TODO
    }

}

void vtkITKLabelShapeStatistics::ClearCentroids()
{
  this->Centroids.clear();
}

void vtkITKLabelShapeStatistics::AddCentroid(int value, double centroid[3])
{
  this->Centroids[value] = vtkVector3d(centroid[0], centroid[1], centroid[2]);
}

bool vtkITKLabelShapeStatistics::GetCentroid(int value, double* centroid)
{
  if (this->Centroids.find(value) == this->Centroids.end())
    {
    return false;
    }

  vtkVector3d* vector = &this->Centroids[value];
  centroid[0] = vector->GetX();
  centroid[1] = vector->GetY();
  centroid[2] = vector->GetZ();
  return true;
}

//
//
//
void vtkITKLabelShapeStatistics::SimpleExecute(vtkImageData *input, vtkImageData *vtkNotUsed(output))
{
  vtkDebugMacro(<< "Executing label shape statistics");

  //
  // Initialize and check input
  //
  vtkPointData *pd = input->GetPointData();
  if (pd ==nullptr)
    {
    vtkErrorMacro(<<"PointData is NULL");
    return;
    }
  vtkDataArray *inScalars=pd->GetScalars();
  if ( inScalars == nullptr )
    {
    vtkErrorMacro(<<"Scalars must be defined for island math");
    return;
    }

  if (inScalars->GetNumberOfComponents() == 1 )
    {

////////// These types are not defined in itk ////////////
#undef VTK_TYPE_USE_LONG_LONG
#undef VTK_TYPE_USE___INT64

#define CALL  vtkITKLabelShapeStatisticsExecute(this, input, this->Directions, static_cast<VTK_TT *>(inPtr));

    void* inPtr = input->GetScalarPointer();
    switch (inScalars->GetDataType())
      {
      vtkTemplateMacroCase(VTK_LONG, long, CALL);                               \
      vtkTemplateMacroCase(VTK_UNSIGNED_LONG, unsigned long, CALL);             \
      vtkTemplateMacroCase(VTK_INT, int, CALL);                                 \
      vtkTemplateMacroCase(VTK_UNSIGNED_INT, unsigned int, CALL);               \
      vtkTemplateMacroCase(VTK_SHORT, short, CALL);                             \
      vtkTemplateMacroCase(VTK_UNSIGNED_SHORT, unsigned short, CALL);           \
      vtkTemplateMacroCase(VTK_CHAR, char, CALL);                               \
      vtkTemplateMacroCase(VTK_SIGNED_CHAR, signed char, CALL);                 \
      vtkTemplateMacroCase(VTK_UNSIGNED_CHAR, unsigned char, CALL);             \
      default:
        {
        vtkErrorMacro(<< "Incompatible data type for this version of ITK.");
        }
      } //switch
    }
  else
    {
    vtkErrorMacro(<< "Only single component images supported.");
    }
}
