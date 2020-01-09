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
#include <vtkObjectFactory.h>
#include <vtkPoints.h>
#include <vtkPointData.h>

// ITK includes
#include <itkLabelImageToShapeLabelMapFilter.h>
#include <itkShapeLabelObject.h>
#include <itkVTKImageToImageFilter.h>

vtkStandardNewMacro(vtkITKLabelShapeStatistics);

//----------------------------------------------------------------------------
vtkITKLabelShapeStatistics::vtkITKLabelShapeStatistics()
{
  this->ComputeFeretDiameter = false;
  this->ComputeOrientedBoundingBox = false;
  this->ComputePerimeter = false;
  this->Directions = nullptr;
}

//----------------------------------------------------------------------------
vtkITKLabelShapeStatistics::~vtkITKLabelShapeStatistics()
{
  this->SetDirections(nullptr);
}

//----------------------------------------------------------------------------
void vtkITKLabelShapeStatistics::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  os << indent << "ComputeFeretDiameter: " << ComputeFeretDiameter << std::endl;
  os << indent << "ComputeOrientedBoundingBox: " << ComputeOrientedBoundingBox << std::endl;
  os << indent << "ComputePerimeter: " << ComputePerimeter << std::endl;
}

//----------------------------------------------------------------------------
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

//----------------------------------------------------------------------------
template <class T>
void vtkITKLabelShapeStatisticsExecute(vtkITKLabelShapeStatistics* self, vtkImageData* input,
  vtkMatrix4x4* directionMatrix, T* vtkNotUsed(inPtr))
{
  if (!self || !input)
    {
    return;
    }

  // Clear current results
  self->ClearCentroids();
  self->ClearOrientedBoundingBox();
  self->ClearFeretDiameter();
  self->ClearPerimeter();
  self->ClearRoundness();
  self->ClearFlatness();

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
  const std::vector<typename ShapeLabelObjectType::LabelType> labelValues = labelShapeObject->GetLabels();

  self->ClearCentroids();
  for (int i = 0; i < labelValues.size(); ++i)
    {
    int labelValue = labelValues[i];

    ShapeLabelObjectType::ShapeLabelObject::Pointer labelObject = labelShapeObject->GetLabelObject(labelValue);
    if (!labelObject)
      {
      continue;
      }

    ShapeLabelObjectType::CentroidType centroidObject = labelObject->GetCentroid();
    self->AddCentroid(labelValue, vtkVector3d(centroidObject[0], centroidObject[1], centroidObject[2]));

    if (self->GetComputeOrientedBoundingBox())
      {
      vtkNew<vtkMatrix4x4> directions;
      ShapeLabelObjectType::OrientedBoundingBoxDirectionType boundingBoxDirections = labelObject->GetOrientedBoundingBoxDirection();
      vtkVector3d origin;
      ShapeLabelObjectType::OrientedBoundingBoxPointType boundingBoxOrigin = labelObject->GetOrientedBoundingBoxOrigin();
      vtkVector3d size;
      ShapeLabelObjectType::OrientedBoundingBoxSizeType boundingBoxSize = labelObject->GetOrientedBoundingBoxSize();
      for (int j=0; j < 3; ++j)
        {
        for (int i = 0; i < 3; ++i)
          {
          directions->SetElement(i, j, boundingBoxDirections(i, j));
          }
        origin[j] = boundingBoxOrigin[j];
        size[j] = boundingBoxSize[j];
        }

      vtkNew<vtkPoints> points;
      /// TODO: In the current version of ITK used in Slicer, the vertices from in ShapeLabelObject are not calculated correctly.

      ///////////////////////
      /// Uncomment when the version of ITK used includes https://github.com/InsightSoftwareConsortium/ITK/pull/1235/
      //ShapeLabelObjectType::OrientedBoundingBoxVerticesType boundingBoxVertices = labelObject->GetOrientedBoundingBoxVertices();
      //for (int i = 0; i < ShapeLabelObjectType::OrientedBoundingBoxVerticesType::Length; ++i)
      //  {
      //  ShapeLabelObjectType::OrientedBoundingBoxPointType vertex = boundingBoxVertices[i];
      //  double point[3] = { vertex[0], vertex[1], vertex[2] };
      //  points->InsertNextPoint(point);
      //  }
      ///////////////////////

      ///////////////////////
      /// Delete when the version of ITK used includes https://github.com/InsightSoftwareConsortium/ITK/pull/1235/
      std::cout << boundingBoxDirections << std::endl;
      const ShapeLabelObjectType::MatrixType obbToPhysical(boundingBoxDirections.GetTranspose());
      for (unsigned int i = 0; i < ShapeLabelObjectType::OrientedBoundingBoxVerticesType::Length; ++i)
        {
        constexpr unsigned int         msb = 1 << (3 - 1);
        ShapeLabelObjectType::OrientedBoundingBoxSizeType offset;
        for (unsigned int j = 0; j < 3; ++j)
          {
          if (i & msb >> j)
            {
            offset[j] = boundingBoxSize[j];
            }
          else
            {
            offset[j] = 0;
            }
          }
          ShapeLabelObjectType::OrientedBoundingBoxPointType vertex = boundingBoxOrigin + (obbToPhysical * offset);
          double point[3] = { vertex[0], vertex[1], vertex[2] };
          points->InsertNextPoint(point);
        }
      ///////////////////////

      self->AddBoundingBox(labelValue, directions, origin, size, points);
      }

    double roundness = labelObject->GetRoundness();
    self->AddRoundness(labelValue, roundness);

    double flatness = labelObject->GetFlatness();
    self->AddFlatness(labelValue, flatness);

    if (self->GetComputeFeretDiameter())
      {
      double feretDiameter = labelObject->GetFeretDiameter();
      self->AddFeretDiameter(labelValue, feretDiameter);
      }

    if (self->GetComputePerimeter())
      {
      double perimeter = labelObject->GetPerimeter();
      self->AddPerimeter(labelValue, perimeter);
      }
    }
}

//----------------------------------------------------------------------------
void vtkITKLabelShapeStatistics::ClearCentroids()
{
  this->Centroids.clear();
}

//----------------------------------------------------------------------------
void vtkITKLabelShapeStatistics::AddCentroid(int value, vtkVector3d centroid)
{
  this->Centroids[value] = centroid;
}

//----------------------------------------------------------------------------
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

//----------------------------------------------------------------------------
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

//----------------------------------------------------------------------------
void vtkITKLabelShapeStatistics::ClearOrientedBoundingBox()
{
  this->OrientedBoundingBoxDirection.clear();
  this->OrientedBoundingBoxOrigin.clear();
  this->OrientedBoundingBoxSize.clear();
  this->OrientedBoundingBoxVertices.clear();
}

//----------------------------------------------------------------------------
void vtkITKLabelShapeStatistics::AddBoundingBox(int labelValue, vtkMatrix4x4* directions, vtkVector3d origin, vtkVector3d size, vtkPoints* points)
{
  this->OrientedBoundingBoxDirection[labelValue] = directions;
  this->OrientedBoundingBoxOrigin[labelValue] = origin;
  this->OrientedBoundingBoxSize[labelValue] = size;
  this->OrientedBoundingBoxVertices[labelValue] = points;
}

//----------------------------------------------------------------------------
void vtkITKLabelShapeStatistics::GetOrientedBoundingBoxDirection(int labelValue, vtkMatrix4x4* directions)
{
  if (!directions)
    {
    return;
    }

  std::map<int, vtkSmartPointer<vtkMatrix4x4> >::iterator directionIt = this->OrientedBoundingBoxDirection.find(labelValue);
  if (directionIt == this->OrientedBoundingBoxDirection.end())
    {
    return;
    }
  directions->DeepCopy(directionIt->second);
}

//----------------------------------------------------------------------------
void vtkITKLabelShapeStatistics::GetOrientedBoundingBoxOrigin(int labelValue, double* origin)
{
  if (!origin)
    {
    return;
    }
  std::map<int, vtkVector3d>::iterator originIt = this->OrientedBoundingBoxOrigin.find(labelValue);
  if (originIt == this->OrientedBoundingBoxOrigin.end())
    {
    return;
    }
  for (int i = 0; i < 3; ++i)
    {
    origin[i] = originIt->second[i];
    }
}

//----------------------------------------------------------------------------
void vtkITKLabelShapeStatistics::GetOrientedBoundingBoxSize(int labelValue, double* size)
{
  if (!size)
    {
    return;
    }
  std::map<int, vtkVector3d>::iterator sizeIt = this->OrientedBoundingBoxSize.find(labelValue);
  if (sizeIt == this->OrientedBoundingBoxSize.end())
    {
    return;
    }
  for (int i = 0; i < 3; ++i)
    {
    size[i] = sizeIt->second[i];
    }
}

//----------------------------------------------------------------------------
void vtkITKLabelShapeStatistics::GetOrientedBoundingBoxVertices(int labelValue, vtkPoints* points)
{
  if (!points)
    {
    return;
    }
  std::map<int, vtkSmartPointer<vtkPoints> >::iterator vertexIt = this->OrientedBoundingBoxVertices.find(labelValue);
  if (vertexIt == this->OrientedBoundingBoxVertices.end())
    {
    return;
    }
  points->DeepCopy(vertexIt->second);
}

//----------------------------------------------------------------------------
void vtkITKLabelShapeStatistics::ClearFeretDiameter()
{
  this->FeretDiameter.clear();
}

//----------------------------------------------------------------------------
void vtkITKLabelShapeStatistics::AddFeretDiameter(int labelValue, double feretDiameter)
{
  this->FeretDiameter[labelValue] = feretDiameter;
}

//----------------------------------------------------------------------------
double vtkITKLabelShapeStatistics::GetFeretDiameter(int labelValue)
{
  std::map<int, double>::iterator feretDiameterIt = this->FeretDiameter.find(labelValue);
  if (feretDiameterIt == this->FeretDiameter.end())
    {
    return 0.0;
    }
  return feretDiameterIt->second;
}

//----------------------------------------------------------------------------
void vtkITKLabelShapeStatistics::ClearPerimeter()
{
  this->Perimeter.clear();
}

//----------------------------------------------------------------------------
void vtkITKLabelShapeStatistics::AddPerimeter(int labelValue, double perimeter)
{
  this->Perimeter[labelValue] = perimeter;
}

//----------------------------------------------------------------------------
double vtkITKLabelShapeStatistics::GetPerimeter(int labelValue)
{
  std::map<int, double>::iterator perimeterIt = this->Perimeter.find(labelValue);
  if (perimeterIt == this->Perimeter.end())
    {
    return 0.0;
    }
  return perimeterIt->second;
}

//----------------------------------------------------------------------------
void vtkITKLabelShapeStatistics::ClearRoundness()
{
  this->Roundness.clear();
}

//----------------------------------------------------------------------------
void vtkITKLabelShapeStatistics::AddRoundness(int labelValue, double roundness)
{
  this->Roundness[labelValue] = roundness;
}

//----------------------------------------------------------------------------
double vtkITKLabelShapeStatistics::GetRoundness(int labelValue)
{
  std::map<int, double>::iterator roundnessIt = this->Roundness.find(labelValue);
  if (roundnessIt == this->Roundness.end())
    {
    return 0.0;
    }
  return roundnessIt->second;
}

//----------------------------------------------------------------------------
void vtkITKLabelShapeStatistics::ClearFlatness()
{
  this->Flatness.clear();
}

//----------------------------------------------------------------------------
void vtkITKLabelShapeStatistics::AddFlatness(int labelValue, double flatness)
{
  this->Flatness[labelValue] = flatness;
}

//----------------------------------------------------------------------------
double vtkITKLabelShapeStatistics::GetFlatness(int labelValue)
{
  std::map<int, double>::iterator flatnessIt = this->Flatness.find(labelValue);
  if (flatnessIt == this->Flatness.end())
  {
    return 0.0;
  }
  return flatnessIt->second;
}
