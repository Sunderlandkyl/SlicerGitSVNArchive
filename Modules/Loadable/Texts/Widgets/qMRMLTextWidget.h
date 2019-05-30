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

  This file was originally developed by Kyle Sunderland, PerkLab, Queen's University
  and was supported through CANARIE's Research Software Program, and Cancer
  Care Ontario.

==============================================================================*/

#ifndef __qMRMLTextWidget_h
#define __qMRMLTextWidget_h

// Qt includes
#include "qSlicerWidget.h"

#include "qMRMLUtils.h"

// Tables Widgets includes
#include "qSlicerTextsModuleWidgetsExport.h"
#include "ui_qMRMLTextWidget.h"

class vtkMRMLTextNode;
class qMRMLTextWidgetPrivate;

/// \ingroup Slicer_QtModules_Texts
class Q_SLICER_MODULE_TEXTS_WIDGETS_EXPORT qMRMLTextWidget : public qSlicerWidget
{
  Q_OBJECT

public:
  typedef qSlicerWidget Superclass;
  qMRMLTextWidget(QWidget *parent=nullptr);
  ~qMRMLTextWidget() override;

  /// Get the table node the columns are edited of.
  Q_INVOKABLE vtkMRMLTextNode* mrmlTextNode() const;

public slots:

  void setMRMLScene(vtkMRMLScene* scene) override;

  void setMRMLTextNode(vtkMRMLTextNode* textNode);
  /// Utility function to simply connect signals/slots with Qt Designer
  void setMRMLTextNode(vtkMRMLNode* textNode);

  void onEditClicked();
  void onCancelClicked();
  void onSaveClicked();

protected slots:

  /// Update the GUI to reflect the currently selected text node.
  void updateWidget();

signals:

  /// This signal is emitted if updates to the widget have finished.
  void updateFinished();

protected:
  QScopedPointer<qMRMLTextWidgetPrivate> d_ptr;

  virtual void setup();

private:
  Q_DECLARE_PRIVATE(qMRMLTextWidget);
  Q_DISABLE_COPY(qMRMLTextWidget);

};

#endif
