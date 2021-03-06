/***************************************************************************
                          qgslabelpropertydialog.cpp
                          --------------------------
    begin                : 2010-11-12
    copyright            : (C) 2010 by Marco Hugentobler
    email                : marco dot hugentobler at sourcepole dot ch
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgslabelpropertydialog.h"
#include <qgsdatadefined.h>
#include "qgsmaplayerregistry.h"
#include "qgsmaprenderer.h"
#include "qgsvectorlayer.h"
#include <QColorDialog>
#include <QFontDialog>

QgsLabelPropertyDialog::QgsLabelPropertyDialog( const QString& layerId, int featureId, const QString& labelText, QgsMapRenderer* renderer, QWidget * parent, Qt::WindowFlags f ):
    QDialog( parent, f ), mMapRenderer( renderer ), mCurLabelField( -1 )
{
  setupUi( this );
  fillHaliComboBox();
  fillValiComboBox();
  init( layerId, featureId, labelText );
}

QgsLabelPropertyDialog::~QgsLabelPropertyDialog()
{
}

void QgsLabelPropertyDialog::init( const QString& layerId, int featureId, const QString& labelText )
{
  if ( !mMapRenderer )
  {
    return;
  }

  //get feature attributes
  QgsVectorLayer* vlayer = dynamic_cast<QgsVectorLayer*>( QgsMapLayerRegistry::instance()->mapLayer( layerId ) );
  if ( !vlayer )
  {
    return;
  }

  if ( !vlayer->getFeatures( QgsFeatureRequest().setFilterFid( featureId ).setFlags( QgsFeatureRequest::NoGeometry ) ).nextFeature( mCurLabelFeat ) )
  {
    return;
  }
  const QgsAttributes& attributeValues = mCurLabelFeat.attributes();

  //get layerproperties. Problem: only for pallabeling...
  QgsPalLabeling* lbl = dynamic_cast<QgsPalLabeling*>( mMapRenderer->labelingEngine() );
  if ( !lbl )
  {
    return;
  }

  blockElementSignals( true );

  QgsPalLayerSettings& layerSettings = lbl->layer( layerId );

  //get label field and fill line edit
  if ( layerSettings.isExpression && !labelText.isNull() )
  {
    mLabelTextLineEdit->setText( labelText );
    mLabelTextLineEdit->setEnabled( false );
    mLabelTextLabel->setText( tr( "Expression result" ) );
  }
  else
  {
    QString labelFieldName = vlayer->customProperty( "labeling/fieldName" ).toString();
    if ( !labelFieldName.isEmpty() )
    {
      mCurLabelField = vlayer->fieldNameIndex( labelFieldName );
      mLabelTextLineEdit->setText( attributeValues[mCurLabelField].toString() );
      const QgsFields& layerFields = vlayer->pendingFields();
      switch ( layerFields[mCurLabelField].type() )
      {
        case QVariant::Double:
          mLabelTextLineEdit->setValidator( new QDoubleValidator( this ) );
          break;
        case QVariant::Int:
        case QVariant::UInt:
        case QVariant::LongLong:
          mLabelTextLineEdit->setValidator( new QIntValidator( this ) );
          break;
        default:
          break;
      }
    }
  }

  //get attributes of the feature and fill data defined values
  mLabelFont = layerSettings.textFont;

  //set all the gui elements to the default values
  mFontSizeSpinBox->setValue( layerSettings.textFont.pointSizeF() );
  mBufferColorButton->setColor( layerSettings.textColor );
  mLabelDistanceSpinBox->setValue( layerSettings.dist );
  mBufferSizeSpinBox->setValue( layerSettings.bufferSize );
  mMinScaleSpinBox->setValue( layerSettings.scaleMin );
  mMaxScaleSpinBox->setValue( layerSettings.scaleMax );
  mHaliComboBox->setCurrentIndex( mHaliComboBox->findText( "Left" ) );
  mValiComboBox->setCurrentIndex( mValiComboBox->findText( "Bottom" ) );
  mFontColorButton->setColorDialogTitle( tr( "Font color" ) );
  mBufferColorButton->setColorDialogTitle( tr( "Buffer color" ) );

  disableGuiElements();

  bool fontEditing = false;

  mDataDefinedProperties = layerSettings.dataDefinedProperties;
  QMap< QgsPalLayerSettings::DataDefinedProperties, QgsDataDefined* >::const_iterator propIt = mDataDefinedProperties.constBegin();

  for ( ; propIt != mDataDefinedProperties.constEnd(); ++propIt )
  {
    QgsDataDefined* dd = propIt.value();
    QString ddField = dd->field();
    if ( !dd->isActive() || dd->useExpression() || ddField.isEmpty() )
    {
      continue; // can only modify attributes with an active data definition of a mapped field
    }

    int ddIndx = vlayer->fieldNameIndex( ddField );
    if ( ddIndx == -1 )
    {
      continue;
    }

    bool ok = false;
    switch ( propIt.key() )
    {
      case QgsPalLayerSettings::Show:
      { // new scope to assign variables
        mShowLabelChkbx->setEnabled( true );
        int showLabel = mCurLabelFeat.attribute( ddIndx ).toInt( &ok );
        mShowLabelChkbx->setChecked( !ok || showLabel != 0 );
        break;
      }
      case QgsPalLayerSettings::AlwaysShow:
        mAlwaysShowChkbx->setEnabled( true );
        mAlwaysShowChkbx->setChecked( mCurLabelFeat.attribute( ddIndx ).toBool() );
        break;
      case QgsPalLayerSettings::MinScale:
      {
        mMinScaleSpinBox->setEnabled( true );
        int minScale = mCurLabelFeat.attribute( ddIndx ).toInt( &ok );
        if ( ok )
        {
          mMinScaleSpinBox->setValue( minScale );
        }
        break;
      }
      case QgsPalLayerSettings::MaxScale:
      {
        mMaxScaleSpinBox->setEnabled( true );
        int maxScale = mCurLabelFeat.attribute( ddIndx ).toInt( &ok );
        if ( ok )
        {
          mMaxScaleSpinBox->setValue( maxScale );
        }
        break;
      }
      case QgsPalLayerSettings::Size:
      {
        mFontSizeSpinBox->setEnabled( true );
        double fontSize = mCurLabelFeat.attribute( ddIndx ).toDouble( &ok );
        if ( ok )
        {
          mLabelFont.setPointSizeF( fontSize );
          mFontSizeSpinBox->setValue( fontSize );
          fontEditing = true;
        }
        break;
      }
      case QgsPalLayerSettings::BufferSize:
      {
        mBufferSizeSpinBox->setEnabled( true );
        double bufferSize = mCurLabelFeat.attribute( ddIndx ).toDouble( &ok );
        if ( ok )
        {
          mBufferSizeSpinBox->setValue( bufferSize );
        }
        break;
      }
      case QgsPalLayerSettings::PositionX:
      {
        mXCoordSpinBox->setEnabled( true );
        double posX = mCurLabelFeat.attribute( ddIndx ).toDouble( &ok );
        if ( ok )
        {
          mXCoordSpinBox->setValue( posX );
        }
        break;
      }
      case QgsPalLayerSettings::PositionY:
      {
        mYCoordSpinBox->setEnabled( true );
        double posY = mCurLabelFeat.attribute( ddIndx ).toDouble( &ok );
        if ( ok )
        {
          mYCoordSpinBox->setValue( posY );
        }
        break;
      }
      case QgsPalLayerSettings::LabelDistance:
      {
        mLabelDistanceSpinBox->setEnabled( true );
        double labelDist = mCurLabelFeat.attribute( ddIndx ).toDouble( &ok );
        if ( ok )
        {
          mLabelDistanceSpinBox->setValue( labelDist );
        }
        break;
      }
      case QgsPalLayerSettings::Hali:
        mHaliComboBox->setEnabled( true );
        mHaliComboBox->setCurrentIndex( mHaliComboBox->findText( mCurLabelFeat.attribute( ddIndx ).toString(), Qt::MatchFixedString ) );
        break;
      case QgsPalLayerSettings::Vali:
        mValiComboBox->setEnabled( true );
        mValiComboBox->setCurrentIndex( mValiComboBox->findText( mCurLabelFeat.attribute( ddIndx ).toString(), Qt::MatchFixedString ) );
        break;
      case QgsPalLayerSettings::BufferColor:
        mBufferColorButton->setEnabled( true );
        mBufferColorButton->setColor( QColor( mCurLabelFeat.attribute( ddIndx ).toString() ) );
        break;
      case QgsPalLayerSettings::Color:
        mFontColorButton->setEnabled( true );
        mFontColorButton->setColor( QColor( mCurLabelFeat.attribute( ddIndx ).toString() ) );
        break;
      case QgsPalLayerSettings::Rotation:
      {
        mRotationSpinBox->setEnabled( true );
        double rot = mCurLabelFeat.attribute( ddIndx ).toDouble( &ok );
        if ( ok )
        {
          mRotationSpinBox->setValue( rot );
        }
        break;
      }

      //font related properties
      case QgsPalLayerSettings::Bold:
        mLabelFont.setBold( mCurLabelFeat.attribute( ddIndx ).toBool() );
        fontEditing = true;
        break;
      case QgsPalLayerSettings::Italic:
        mLabelFont.setItalic( mCurLabelFeat.attribute( ddIndx ).toBool() );
        fontEditing = true;
        break;
      case QgsPalLayerSettings::Underline:
        mLabelFont.setUnderline( mCurLabelFeat.attribute( ddIndx ).toBool() );
        fontEditing = true;
        break;
      case QgsPalLayerSettings::Strikeout:
        mLabelFont.setStrikeOut( mCurLabelFeat.attribute( ddIndx ).toBool() );
        fontEditing = true;
        break;
      case QgsPalLayerSettings::Family:
        mLabelFont.setFamily( mCurLabelFeat.attribute( ddIndx ).toString() );
        fontEditing = true;
        break;
      default:
        break;
    }
  }
  mFontPushButton->setEnabled( fontEditing );
  blockElementSignals( false );
}

void QgsLabelPropertyDialog::disableGuiElements()
{
  mShowLabelChkbx->setEnabled( false );
  mAlwaysShowChkbx->setEnabled( false );
  mMinScaleSpinBox->setEnabled( false );
  mMaxScaleSpinBox->setEnabled( false );
  mFontSizeSpinBox->setEnabled( false );
  mBufferSizeSpinBox->setEnabled( false );
  mFontPushButton->setEnabled( false );
  mFontColorButton->setEnabled( false );
  mBufferColorButton->setEnabled( false );
  mLabelDistanceSpinBox->setEnabled( false );
  mXCoordSpinBox->setEnabled( false );
  mYCoordSpinBox->setEnabled( false );
  mHaliComboBox->setEnabled( false );
  mValiComboBox->setEnabled( false );
  mRotationSpinBox->setEnabled( false );
}

void QgsLabelPropertyDialog::blockElementSignals( bool block )
{
  mShowLabelChkbx->blockSignals( block );
  mAlwaysShowChkbx->blockSignals( block );
  mMinScaleSpinBox->blockSignals( block );
  mMaxScaleSpinBox->blockSignals( block );
  mFontSizeSpinBox->blockSignals( block );
  mBufferSizeSpinBox->blockSignals( block );
  mFontPushButton->blockSignals( block );
  mFontColorButton->blockSignals( block );
  mBufferColorButton->blockSignals( block );
  mLabelDistanceSpinBox->blockSignals( block );
  mXCoordSpinBox->blockSignals( block );
  mYCoordSpinBox->blockSignals( block );
  mHaliComboBox->blockSignals( block );
  mValiComboBox->blockSignals( block );
  mRotationSpinBox->blockSignals( block );
}

void QgsLabelPropertyDialog::fillHaliComboBox()
{
  mHaliComboBox->addItem( "Left" );
  mHaliComboBox->addItem( "Center" );
  mHaliComboBox->addItem( "Right" );
}

void QgsLabelPropertyDialog::fillValiComboBox()
{
  mValiComboBox->addItem( "Bottom" );
  mValiComboBox->addItem( "Base" );
  mValiComboBox->addItem( "Half" );
  mValiComboBox->addItem( "Top" );
}

void QgsLabelPropertyDialog::on_mShowLabelChkbx_toggled( bool chkd )
{
  insertChangedValue( QgsPalLayerSettings::Show, ( chkd ? 1 : 0 ) );
}

void QgsLabelPropertyDialog::on_mAlwaysShowChkbx_toggled( bool chkd )
{
  insertChangedValue( QgsPalLayerSettings::AlwaysShow, ( chkd ? 1 : 0 ) );
}

void QgsLabelPropertyDialog::on_mMinScaleSpinBox_valueChanged( int i )
{
  insertChangedValue( QgsPalLayerSettings::MinScale, i );
}

void QgsLabelPropertyDialog::on_mMaxScaleSpinBox_valueChanged( int i )
{
  insertChangedValue( QgsPalLayerSettings::MaxScale, i );
}

void QgsLabelPropertyDialog::on_mLabelDistanceSpinBox_valueChanged( double d )
{
  insertChangedValue( QgsPalLayerSettings::LabelDistance, d );
}

void QgsLabelPropertyDialog::on_mXCoordSpinBox_valueChanged( double d )
{
  insertChangedValue( QgsPalLayerSettings::PositionX, d );
}

void QgsLabelPropertyDialog::on_mYCoordSpinBox_valueChanged( double d )
{
  insertChangedValue( QgsPalLayerSettings::PositionY, d );
}

void QgsLabelPropertyDialog::on_mFontSizeSpinBox_valueChanged( double d )
{
  insertChangedValue( QgsPalLayerSettings::Size, d );
}

void QgsLabelPropertyDialog::on_mBufferSizeSpinBox_valueChanged( double d )
{
  insertChangedValue( QgsPalLayerSettings::PositionX, d );
}

void QgsLabelPropertyDialog::on_mRotationSpinBox_valueChanged( double d )
{
  insertChangedValue( QgsPalLayerSettings::Rotation, d );
}

void QgsLabelPropertyDialog::on_mFontPushButton_clicked()
{
  bool ok;
#if defined(Q_WS_MAC) && QT_VERSION >= 0x040500 && defined(QT_MAC_USE_COCOA)
  // Native Mac dialog works only for Qt Carbon
  mLabelFont = QFontDialog::getFont( &ok, mLabelFont, 0, tr( "Label font" ), QFontDialog::DontUseNativeDialog );
#else
  mLabelFont = QFontDialog::getFont( &ok, mLabelFont, 0, tr( "Label font" ) );
#endif
  if ( ok )
  {
    insertChangedValue( QgsPalLayerSettings::Size, mLabelFont.pointSizeF() );
    insertChangedValue( QgsPalLayerSettings::Bold, mLabelFont.bold() );
    insertChangedValue( QgsPalLayerSettings::Italic, mLabelFont.italic() );
    insertChangedValue( QgsPalLayerSettings::Underline, mLabelFont.underline() );
    insertChangedValue( QgsPalLayerSettings::Strikeout, mLabelFont.strikeOut() );
  }
}

void QgsLabelPropertyDialog::on_mFontColorButton_colorChanged( const QColor &color )
{
  insertChangedValue( QgsPalLayerSettings::Color, color.name() );
}

void QgsLabelPropertyDialog::on_mBufferColorButton_colorChanged( const QColor &color )
{
  insertChangedValue( QgsPalLayerSettings::BufferColor, color.name() );
}

void QgsLabelPropertyDialog::on_mHaliComboBox_currentIndexChanged( const QString& text )
{
  insertChangedValue( QgsPalLayerSettings::Hali, text );
}

void QgsLabelPropertyDialog::on_mValiComboBox_currentIndexChanged( const QString& text )
{
  insertChangedValue( QgsPalLayerSettings::Vali, text );
}

void QgsLabelPropertyDialog::on_mLabelTextLineEdit_textChanged( const QString& text )
{
  if ( mCurLabelField != -1 )
  {
    mChangedProperties.insert( mCurLabelField, text );
  }
}

void QgsLabelPropertyDialog::insertChangedValue( QgsPalLayerSettings::DataDefinedProperties p, QVariant value )
{
  QMap< QgsPalLayerSettings::DataDefinedProperties, QgsDataDefined* >::const_iterator ddIt = mDataDefinedProperties.find( p );
  if ( ddIt != mDataDefinedProperties.constEnd() )
  {
    QgsDataDefined* dd = ddIt.value();

    if ( dd->isActive() && !dd->useExpression() && !dd->field().isEmpty() )
    {
      mChangedProperties.insert( mCurLabelFeat.fieldNameIndex( dd->field() ), value );
    }
  }
}
