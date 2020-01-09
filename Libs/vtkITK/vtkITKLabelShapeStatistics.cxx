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
  this->Directions = nullptr;
  this->StatisticsTable = nullptr;

  this->ComputedStatistics.insert(Centroid);
  this->ComputedStatistics.insert(OrientedBoundingBox);
  this->ComputedStatistics.insert(FeretDiameter);
  this->ComputedStatistics.insert(Perimeter);
  this->ComputedStatistics.insert(Roundness);
  this->ComputedStatistics.insert(Flatness); // TODO: Disable stats that are not calculated by default
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
}

//----------------------------------------------------------------------------
std::string vtkITKLabelShapeStatistics::GetShapeStatisticAsString(ShapeStatistic statistic)
{
  switch (statistic)
    {
    case Centroid:
      return "Centroid";
    case OrientedBoundingBox:
      return "OrientedBoundingBox";
    case FeretDiameter:
      return "FeretDiameter";
    case Perimeter:
      return "Perimeter";
    case Roundness:
      return "Roundness";
    case Flatness:
      return "Flatness";
    default:
      return "";
    }
}

//----------------------------------------------------------------------------
void vtkITKLabelShapeStatistics::ComputeShapeStatisticOn(ShapeStatistic statistic)
{
  this->SetComputeShapeStatistic(this->GetShapeStatisticAsString(statistic), true);
}


//----------------------------------------------------------------------------
void vtkITKLabelShapeStatistics::ComputeShapeStatisticOn(std::string statisticName)
{
  this->SetComputeShapeStatistic(statisticName, true);
}

//----------------------------------------------------------------------------
void vtkITKLabelShapeStatistics::ComputeShapeStatisticOff(ShapeStatistic statistic)
{
  this->SetComputeShapeStatistic(this->GetShapeStatisticAsString(statistic), false);
}

//----------------------------------------------------------------------------
void vtkITKLabelShapeStatistics::ComputeShapeStatisticOff(std::string statisticName)
{
  this->SetComputeShapeStatistic(statisticName, false);
}

//----------------------------------------------------------------------------
void vtkITKLabelShapeStatistics::SetComputeShapeStatistic(ShapeStatistic statistic, bool state)
{
  this->SetComputeShapeStatistic(this->GetShapeStatisticAsString(statistic), state);
}

//----------------------------------------------------------------------------
void vtkITKLabelShapeStatistics::SetComputeShapeStatistic(std::string statisticName, bool state)
{
  if (!state)
    {
    std::set<std::string>::iterator statIt = this->ComputedStatistics.find(statisticName);
    if (statIt != this->ComputedStatistics.end())
      {
      this->ComputedStatistics.erase(statIt);
      }
    }
  else
    {
    this->ComputedStatistics.insert(statisticName);
    }
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
  vtkNew<vtkTable> table;
  self->SetStatisticsTable(table);

  // Wrap VTK image into an ITK image
  using ImageType = itk::Image<T, 3> ;
  using VTKToITKFilterType = itk::VTKImageToImageFilter<ImageType>;
  typename VTKToITKFilterType::Pointer vtkToITKFilter = VTKToITKFilterType::New();
  vtkToITKFilter->SetInput(input);
  vtkToITKFilter->Update();
  ImageType* inImage = vtkToITKFilter->GetOutput();

  // TODO: When vtkImageData is updated to include direction, this should be updated
  if (directionMatrix)
    {
    typename ImageType::DirectionType gridDirectionMatrix;
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

  using ShapeLabelObjectType = itk::ShapeLabelObject<T, 3>;
  using LabelMapType = itk::LabelMap<ShapeLabelObjectType>;
  using LableShapeFilterType = itk::LabelImageToShapeLabelMapFilter<ImageType, LabelMapType>;
  typename LableShapeFilterType::Pointer labelFilter = LableShapeFilterType::New();
  labelFilter->AddObserver(itk::ProgressEvent(), progressCommand);
  labelFilter->SetInput(inImage);
  //labelFilter->SetComputeFeretDiameter(self->GetComputeFeretDiameter()); // TODO: Enable selected options
  //labelFilter->SetComputePerimeter(self->GetComputePerimeter());
  //labelFilter->SetComputeOrientedBoundingBox(self->GetComputeOrientedBoundingBox());
  labelFilter->SetComputeFeretDiameter(true);
  labelFilter->SetComputePerimeter(true);
  labelFilter->SetComputeOrientedBoundingBox(true);
  labelFilter->Update();

  typename LabelMapType::Pointer labelShapeObject = labelFilter->GetOutput();
  const std::vector<typename ShapeLabelObjectType::LabelType> labelValues = labelShapeObject->GetLabels();

  std::map<std::string, vtkSmartPointer<vtkAbstractArray> > arrays;
  for (unsigned int i = 0; i < labelValues.size(); ++i)
    {
    int labelValue = labelValues[i];

    typename ShapeLabelObjectType::ShapeLabelObject::Pointer labelObject = labelShapeObject->GetLabelObject(labelValue);
    if (!labelObject)
      {
      continue;
      }

    for (std::string statisticName : self->GetComputedStatistics())
      {
      vtkSmartPointer<vtkAbstractArray>

      if (statisticName == self->GetShapeStatisticAsString(vtkITKLabelShapeStatistics::Centroid))
        {
        typename ShapeLabelObjectType::CentroidType centroidObject = labelObject->GetCentroid();
        self->AddCentroid(labelValue, vtkVector3d(centroidObject[0], centroidObject[1], centroidObject[2]));
        }

      }




    //if (self->GetComputeOrientedBoundingBox())
    //  {
    //  vtkNew<vtkMatrix4x4> directions;
    //  typename ShapeLabelObjectType::OrientedBoundingBoxDirectionType boundingBoxDirections = labelObject->GetOrientedBoundingBoxDirection();
    //  vtkVector3d origin;
    //  typename ShapeLabelObjectType::OrientedBoundingBoxPointType boundingBoxOrigin = labelObject->GetOrientedBoundingBoxOrigin();
    //  vtkVector3d size;
    //  typename ShapeLabelObjectType::OrientedBoundingBoxSizeType boundingBoxSize = labelObject->GetOrientedBoundingBoxSize();
    //  for (int j=0; j < 3; ++j)
    //    {
    //    for (int i = 0; i < 3; ++i)
    //      {
    //      directions->SetElement(i, j, boundingBoxDirections(i, j));
    //      }
    //    origin[j] = boundingBoxOrigin[j];
    //    size[j] = boundingBoxSize[j];
    //    }
    //  self->AddBoundingBox(labelValue, directions, origin, size);
    //  }

    //double roundness = labelObject->GetRoundness();
    //self->AddRoundness(labelValue, roundness);

    //double flatness = labelObject->GetFlatness();
    //self->AddFlatness(labelValue, flatness);

    //if (self->GetComputeFeretDiameter())
    //  {
    //  double feretDiameter = labelObject->GetFeretDiameter();
    //  self->AddFeretDiameter(labelValue, feretDiameter);
    //  }

    //if (self->GetComputePerimeter())
    //  {
    //  double perimeter = labelObject->GetPerimeter();
    //  self->AddPerimeter(labelValue, perimeter);
    //  }
    }
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
