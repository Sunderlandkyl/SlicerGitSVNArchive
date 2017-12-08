/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkMarkupsSplineRepresentation.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkMarkupsSplineRepresentation.h"

#include "vtkActor.h"
#include "vtkAssemblyNode.h"
#include "vtkAssemblyPath.h"
#include "vtkBoundingBox.h"
#include "vtkCallbackCommand.h"
#include "vtkCamera.h"
#include "vtkCellArray.h"
#include "vtkCellPicker.h"
#include "vtkInteractorObserver.h"
#include "vtkMath.h"
#include "vtkObjectFactory.h"
#include "vtkParametricFunctionSource.h"
#include "vtkParametricSpline.h"
#include "vtkPickingManager.h"
#include "vtkPlaneSource.h"
#include "vtkPolyData.h"
#include "vtkPolyDataMapper.h"
#include "vtkProperty.h"
#include "vtkRenderer.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkSphereSource.h"
#include "vtkTransform.h"
#include "vtkDoubleArray.h"

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkMarkupsSplineRepresentation);

//----------------------------------------------------------------------------
vtkMarkupsSplineRepresentation::vtkMarkupsSplineRepresentation()
{
  // Build the representation of the widget

  // Create the handles along a straight line within the bounds of a unit cube
  double x0 = -0.5;
  double x1 =  0.5;
  double y0 = -0.5;
  double y1 =  0.5;
  double z0 = -0.5;
  double z1 =  0.5;

  vtkPoints* points = vtkPoints::New(VTK_DOUBLE);
  points->SetNumberOfPoints(this->NumberOfHandles);

  for ( int i = 0; i < this->NumberOfHandles; ++i )
    {
    double u = i / (this->NumberOfHandles - 1.0);
    double x = (1.0 - u)*x0 + u*x1;
    double y = (1.0 - u)*y0 + u*y1;
    double z = (1.0 - u)*z0 + u*z1;
    points->SetPoint(i, x, y, z);
    this->HandleGeometry[i]->SetCenter(x,y,z);
    }

  // vtkParametric spline acts as the interpolating engine
  this->ParametricSpline = vtkParametricSpline::New();
  this->ParametricSpline->Register(this);
  this->ParametricSpline->SetPoints(points);
  points->Delete();
  this->ParametricSpline->Delete();

  // Define the points and line segments representing the spline
  this->Resolution = 499;

  this->ParametricFunctionSource = vtkParametricFunctionSource::New();
  this->ParametricFunctionSource->SetParametricFunction(this->ParametricSpline);
  this->ParametricFunctionSource->SetScalarModeToNone();
  this->ParametricFunctionSource->GenerateTextureCoordinatesOff();
  this->ParametricFunctionSource->SetUResolution( this->Resolution );
  this->ParametricFunctionSource->Update();

  vtkPolyDataMapper* lineMapper = vtkPolyDataMapper::New();
  lineMapper->SetInputConnection(
    this->ParametricFunctionSource->GetOutputPort()) ;
  lineMapper->ImmediateModeRenderingOn();
  lineMapper->SetResolveCoincidentTopologyToPolygonOffset();

  this->LineActor->SetMapper( lineMapper );
  lineMapper->Delete();

}

//----------------------------------------------------------------------------
vtkMarkupsSplineRepresentation::~vtkMarkupsSplineRepresentation()
{
  if ( this->ParametricSpline )
    {
    this->ParametricSpline->UnRegister(this);
    }

  this->ParametricFunctionSource->Delete();
}

//----------------------------------------------------------------------------
void vtkMarkupsSplineRepresentation::SetParametricSpline(vtkParametricSpline* spline)
{
  if ( this->ParametricSpline != spline )
    {
    // to avoid destructor recursion
    vtkParametricSpline *temp = this->ParametricSpline;
    this->ParametricSpline = spline;
    if (temp != NULL)
      {
      temp->UnRegister(this);
      }
    if (this->ParametricSpline != NULL)
      {
      this->ParametricSpline->Register(this);
      this->ParametricFunctionSource->SetParametricFunction(this->ParametricSpline);
      }
    }
}

//----------------------------------------------------------------------------
vtkDoubleArray* vtkMarkupsSplineRepresentation::GetHandlePositions()
{
  return vtkArrayDownCast<vtkDoubleArray>(
    this->ParametricSpline->GetPoints()->GetData());
}

//----------------------------------------------------------------------------
void vtkMarkupsSplineRepresentation::BuildRepresentation()
{
  this->ValidPick = 1;
  // TODO: Avoid unnecessary rebuilds.
  // Handles have changed position, re-compute the spline coeffs
  vtkPoints* points = this->ParametricSpline->GetPoints();
  if ( points->GetNumberOfPoints() != this->NumberOfHandles )
    {
    points->SetNumberOfPoints( this->NumberOfHandles );
    }

  vtkBoundingBox bbox;
  for ( int i = 0; i < this->NumberOfHandles; ++i )
    {
    double pt[3];
    this->HandleGeometry[i]->GetCenter(pt);
    points->SetPoint(i, pt);
    bbox.AddPoint(pt);
    }
  this->ParametricSpline->SetClosed(this->Closed);
  this->ParametricSpline->Modified();

  double bounds[6];
  bbox.GetBounds(bounds);
  this->InitialLength = sqrt((bounds[1]-bounds[0])*(bounds[1]-bounds[0]) +
                             (bounds[3]-bounds[2])*(bounds[3]-bounds[2]) +
                             (bounds[5]-bounds[4])*(bounds[5]-bounds[4]));
  this->SizeHandles();
}

//----------------------------------------------------------------------------
void vtkMarkupsSplineRepresentation::SetNumberOfHandles(int npts)
{
  if ( this->NumberOfHandles == npts )
    {
    return;
    }
  if (npts < 1)
    {
    vtkGenericWarningMacro(<<"vtkMarkupsSplineRepresentation: minimum of 1 points required.");
    return;
    }

  // Ensure that no handle is current
  this->HighlightHandle(NULL);

  double radius = this->HandleGeometry[0]->GetRadius();
  this->Initialize();

  this->NumberOfHandles = npts;

  // Create the handles

  for ( int i = 0; i < this->NumberOfHandles; ++i )
    {
    if (this->HandleGeometry.size() <= i)
      {
      this->HandleGeometry.push_back(vtkSmartPointer<vtkSphereSource>::New());
      }
    this->HandleGeometry[i]->SetThetaResolution(16);
    this->HandleGeometry[i]->SetPhiResolution(8);
    vtkPolyDataMapper* handleMapper = vtkPolyDataMapper::New();
    handleMapper->SetInputConnection(
      this->HandleGeometry[i]->GetOutputPort());
    if (this->Handle.size() <= i)
      {
      this->Handle.push_back(vtkActor::New());
      }
    this->Handle[i]->SetMapper(handleMapper);
    handleMapper->Delete();
    this->Handle[i]->SetProperty(this->HandleProperty);
    double u[3], pt[3];
    u[0] = i/(this->NumberOfHandles - 1.0);
    this->ParametricSpline->Evaluate(u, pt, NULL);
    this->HandleGeometry[i]->SetCenter(pt);
    this->HandleGeometry[i]->SetRadius(radius);
    this->HandlePicker->AddPickList(this->Handle[i]);
    }

  if (this->CurrentHandleIndex >= 0 &&
    this->CurrentHandleIndex < this->NumberOfHandles)
    {
    this->CurrentHandleIndex =
      this->HighlightHandle(this->Handle[this->CurrentHandleIndex]);
    }
  else
    {
    this->CurrentHandleIndex = this->HighlightHandle(NULL);
    }

  this->BuildRepresentation();
}

//----------------------------------------------------------------------------
void vtkMarkupsSplineRepresentation::SetResolution(int resolution)
{
  if ( this->Resolution == resolution || resolution < (this->NumberOfHandles-1) )
    {
    return;
    }

  this->Resolution = resolution;
  this->ParametricFunctionSource->SetUResolution( this->Resolution );
  this->ParametricFunctionSource->Modified();
}

//----------------------------------------------------------------------------
void vtkMarkupsSplineRepresentation::GetPolyData(vtkPolyData *pd)
{
  pd->ShallowCopy( this->ParametricFunctionSource->GetOutput() );
}

//----------------------------------------------------------------------------
double vtkMarkupsSplineRepresentation::GetSummedLength()
{
  vtkPoints* points = this->ParametricFunctionSource->GetOutput()->GetPoints();
  int npts = points->GetNumberOfPoints();

  if ( npts < 2 ) { return 0.0; }

  double a[3];
  double b[3];
  double sum = 0.0;
  int i = 0;
  points->GetPoint(i, a);
  int imax = (npts % 2 == 0) ? npts-2 : npts-1;

  while ( i < imax )
    {
    points->GetPoint(i+1, b);
    sum += sqrt(vtkMath::Distance2BetweenPoints(a, b));
    i = i + 2;
    points->GetPoint(i, a);
    sum = sum + sqrt(vtkMath::Distance2BetweenPoints(a, b));
    }

  if ( npts % 2 == 0 )
    {
    points->GetPoint(i+1, b);
    sum += sqrt(vtkMath::Distance2BetweenPoints(a, b));
    }

  return sum;
}

//----------------------------------------------------------------------------
void vtkMarkupsSplineRepresentation::InsertHandleOnLine(double* pos)
{
  if (this->NumberOfHandles < 2) { return; }

  vtkIdType id = this->LinePicker->GetCellId();
  if (id == -1)
    {
    // Didn't click on a line segment
    this->InsertHandle(pos);
    return;
    }

  vtkIdType subid = this->LinePicker->GetSubId();

  //vtkSmartPointer<vtkPoints> newPoints = vtkSmartPointer<vtkPoints>::New();
  vtkPoints* newPoints = vtkPoints::New();
  newPoints->SetDataType(VTK_DOUBLE);
  newPoints->SetNumberOfPoints(this->NumberOfHandles + 1);

  int istart = vtkMath::Floor(subid*(this->NumberOfHandles + this->Closed - 1.0)/
    static_cast<double>(this->Resolution));
  int istop = istart + 1;
  int count = 0;
  for ( int i = 0; i <= istart; ++i )
    {
    newPoints->SetPoint(count++,this->HandleGeometry[i]->GetCenter());
    }

  newPoints->SetPoint(count++, pos);

  for ( int i = istop; i < this->NumberOfHandles; ++i )
    {
    newPoints->SetPoint(count++, this->HandleGeometry[i]->GetCenter());
    }

  this->InitializeHandles(newPoints);
}

void vtkMarkupsSplineRepresentation::InsertHandle(double* pos)
{
  //vtkSmartPointer<vtkPoints> newPoints = vtkSmartPointer<vtkPoints>::New();
  vtkPoints* newPoints = vtkPoints::New();
  newPoints->SetDataType(VTK_DOUBLE);
  newPoints->SetNumberOfPoints(this->NumberOfHandles + 1);

  for (int i = 0; i < this->NumberOfHandles; ++i)
    {
    newPoints->SetPoint(i, this->HandleGeometry[i]->GetCenter());
    }
  newPoints->SetPoint(newPoints->GetNumberOfPoints(), pos);

  this->InitializeHandles(newPoints);
}

//----------------------------------------------------------------------------
void vtkMarkupsSplineRepresentation::InitializeHandles(vtkPoints* points)
{
  if ( !points ){ return; }

  int npts = points->GetNumberOfPoints();
  if ( npts < 2 ){ return; }

  double p0[3];
  double p1[3];

  points->GetPoint(0,p0);
  points->GetPoint(npts-1,p1);

  if ( vtkMath::Distance2BetweenPoints(p0,p1) == 0.0 )
    {
    --npts;
    this->Closed = 1;
    this->ParametricSpline->ClosedOn();
    }

  this->SetNumberOfHandles(npts);
  for ( int i = 0; i < npts; ++i )
    {
    this->SetHandlePosition(i,points->GetPoint(i));
    }
}

//----------------------------------------------------------------------------
void vtkMarkupsSplineRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
  if ( this->ParametricSpline )
    {
    os << indent << "ParametricSpline: "
       << this->ParametricSpline << "\n";
    }
  else
    {
    os << indent << "ParametricSpline: (none)\n";
    }
}