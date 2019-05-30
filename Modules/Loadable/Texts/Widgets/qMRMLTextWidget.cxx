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

// Texts Widgets includes
#include "qMRMLTextWidget.h"

#include <vtkMRMLTextNode.h>

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_CreateModels
class qMRMLTextWidgetPrivate
  : public Ui_qMRMLTextWidget
{
  Q_DECLARE_PUBLIC(qMRMLTextWidget);
protected:
  qMRMLTextWidget* const q_ptr;

public:
  qMRMLTextWidgetPrivate( qMRMLTextWidget& object);
  ~qMRMLTextWidgetPrivate();
  virtual void setupUi(qMRMLTextWidget*);

public:
  vtkWeakPointer<vtkMRMLTextNode> CurrentTextNode;
  bool IsEditing;
};

// --------------------------------------------------------------------------
qMRMLTextWidgetPrivate::qMRMLTextWidgetPrivate( qMRMLTextWidget& object)
  : q_ptr(&object),
  CurrentTextNode(nullptr),
  IsEditing(false)
{
}

//-----------------------------------------------------------------------------
qMRMLTextWidgetPrivate::~qMRMLTextWidgetPrivate() = default;

// --------------------------------------------------------------------------
void qMRMLTextWidgetPrivate::setupUi(qMRMLTextWidget* widget)
{
  this->Ui_qMRMLTextWidget::setupUi(widget);
}


//-----------------------------------------------------------------------------
// qMRMLTextWidget methods

//-----------------------------------------------------------------------------
qMRMLTextWidget::qMRMLTextWidget(QWidget* parentWidget)
  : Superclass( parentWidget )
  , d_ptr(new qMRMLTextWidgetPrivate(*this))
{
  this->setup();
}

//-----------------------------------------------------------------------------
qMRMLTextWidget::~qMRMLTextWidget()
{
  this->setMRMLTextNode((vtkMRMLTextNode*)nullptr);
}

//-----------------------------------------------------------------------------
void qMRMLTextWidget::setup()
{
  Q_D(qMRMLTextWidget);

  d->setupUi(this);

  connect(d->EditButton, SIGNAL(clicked()), this, SLOT(onEditClicked()));
  connect(d->CancelButton, SIGNAL(clicked()), this, SLOT(onCancelClicked()));
  connect(d->SaveButton, SIGNAL(clicked()), this, SLOT(onSaveClicked()));
}

//------------------------------------------------------------------------------
void qMRMLTextWidget::setMRMLTextNode(vtkMRMLNode* node)
{
  this->setMRMLTextNode(vtkMRMLTextNode::SafeDownCast(node));
}

//------------------------------------------------------------------------------
void qMRMLTextWidget::setMRMLTextNode(vtkMRMLTextNode* node)
{
  Q_D(qMRMLTextWidget);
  if (node == d->CurrentTextNode)
    {
    // not changed
    return;
    }

  // Reconnect the appropriate nodes
  this->qvtkReconnect(d->CurrentTextNode, node, vtkCommand::ModifiedEvent, this, SLOT(updateWidget()));
  d->CurrentTextNode = node;

  d->IsEditing = false;
  this->updateWidget();
}

//------------------------------------------------------------------------------
vtkMRMLTextNode* qMRMLTextWidget::mrmlTextNode()const
{
  Q_D(const qMRMLTextWidget);
  return d->CurrentTextNode;
}

//-----------------------------------------------------------------------------
void qMRMLTextWidget::updateWidget()
{
  Q_D(qMRMLTextWidget);

  d->TextEdit->setReadOnly(true);
  d->EditButton->setVisible(true);
  d->EditButton->setEnabled(false);
  d->CancelButton->setEnabled(false);
  d->CancelButton->setVisible(false);
  d->SaveButton->setEnabled(false);
  d->SaveButton->setVisible(false);

  if (!d->CurrentTextNode)
  {
    d->TextEdit->setText("");
    return;
  }

  if (!d->IsEditing)
  {
    d->TextEdit->setText(d->CurrentTextNode->GetText());
  }

  d->TextEdit->setReadOnly(!d->IsEditing);

  d->EditButton->setEnabled(!d->IsEditing);
  d->EditButton->setVisible(!d->IsEditing);

  d->CancelButton->setEnabled(d->IsEditing);
  d->CancelButton->setVisible(d->IsEditing);

  d->SaveButton->setEnabled(d->IsEditing);
  d->SaveButton->setVisible(d->IsEditing);
}

//------------------------------------------------------------------------------
void qMRMLTextWidget::setMRMLScene(vtkMRMLScene* scene)
{
  this->Superclass::setMRMLScene(scene);
  this->updateWidget();
}

//------------------------------------------------------------------------------
void qMRMLTextWidget::onEditClicked()
{
  Q_D(qMRMLTextWidget);
  d->IsEditing = true;
  this->updateWidget();
}

//------------------------------------------------------------------------------
void qMRMLTextWidget::onCancelClicked()
{
  Q_D(qMRMLTextWidget);
  d->IsEditing = false;
  this->updateWidget();
}


//------------------------------------------------------------------------------
void qMRMLTextWidget::onSaveClicked()
{
  Q_D(qMRMLTextWidget);
  d->IsEditing = false;

  if (d->CurrentTextNode)
  {
    std::string text = d->TextEdit->toPlainText().toStdString();
    d->CurrentTextNode->SetText(text.c_str());
  }
  this->updateWidget();
}