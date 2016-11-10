/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2015 - 2016 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
**************************************************************************/

#include "TransferDialog.h"
#include "../core/HandlersManager.h"
#include "../core/ThemesManager.h"
#include "../core/TransfersManager.h"
#include "../core/Utils.h"

#include "ui_TransferDialog.h"

#include <QtCore/QFileInfo>
#include <QtCore/QtMath>

namespace Otter
{

TransferDialog::TransferDialog(Transfer *transfer, QWidget *parent) : Dialog(parent),
	m_transfer(transfer),
	m_ui(new Ui::TransferDialog)
{
	const QPixmap icon(ThemesManager::getIcon(transfer->getMimeType().iconName()).pixmap(16, 16));
	QString fileName(transfer->getSuggestedFileName());

	if (fileName.isEmpty())
	{
		fileName = tr("unknown file");
	}

	m_ui->setupUi(this);

	if (icon.isNull())
	{
		m_ui->iconLabel->hide();
	}
	else
	{
		m_ui->iconLabel->setPixmap(icon);
	}

	m_ui->nameTextLabelWidget->setText(fileName);
	m_ui->typeTextLabelWidget->setText(transfer->getMimeType().comment());
	m_ui->fromTextLabelWidget->setText(transfer->getSource().host().isEmpty() ? QLatin1String("localhost") : transfer->getSource().host());
	m_ui->openWithComboBoxWidget->setMimeType(transfer->getMimeType());

	setProgress(m_transfer->getBytesReceived(), m_transfer->getBytesTotal());
	setWindowTitle(tr("Opening %1").arg(fileName));

	connect(transfer, SIGNAL(progressChanged(qint64,qint64)), this, SLOT(setProgress(qint64,qint64)));
	connect(m_ui->buttonBox, SIGNAL(clicked(QAbstractButton*)), this, SLOT(buttonClicked(QAbstractButton*)));
}

TransferDialog::~TransferDialog()
{
	delete m_ui;
}

void TransferDialog::changeEvent(QEvent *event)
{
	QWidget::changeEvent(event);

	if (event->type() == QEvent::LanguageChange)
	{
		m_ui->retranslateUi(this);
	}
}

void TransferDialog::buttonClicked(QAbstractButton *button)
{
	const QDialogButtonBox::StandardButton standardButton(m_ui->buttonBox->standardButton(button));

	if (!m_transfer || (standardButton != QDialogButtonBox::Open && standardButton != QDialogButtonBox::Save))
	{
		if (m_transfer)
		{
			m_transfer->cancel();

			if (m_ui->rememberChoiceCheckBox->isChecked())
			{
				HandlerDefinition definition;
				definition.transferMode = IgnoreTransferMode;

				HandlersManager::setHandler(m_transfer->getMimeType().name(), definition);
			}
		}

		reject();

		return;
	}

	if (standardButton == QDialogButtonBox::Open)
	{
		m_transfer->setOpenCommand(m_ui->openWithComboBoxWidget->getCommand());

		if (m_ui->rememberChoiceCheckBox->isChecked())
		{
			HandlerDefinition definition;
			definition.transferMode = OpenTransferMode;
			definition.openCommand = m_ui->openWithComboBoxWidget->getCommand();

			HandlersManager::setHandler(m_transfer->getMimeType().name(), definition);
		}
	}
	else if (standardButton == QDialogButtonBox::Save)
	{
		QWidget *dialog(parentWidget());

		while (dialog)
		{
			if (dialog->inherits("Otter::ContentsDialog"))
			{
				dialog->hide();

				break;
			}

			dialog = dialog->parentWidget();
		}

		const QString path(Utils::getSavePath(m_transfer->getSuggestedFileName()).path);

		if (path.isEmpty())
		{
			m_transfer->cancel();

			reject();

			return;
		}

		m_transfer->setTarget(path, true);

		if (m_ui->rememberChoiceCheckBox->isChecked())
		{
			HandlerDefinition definition;
			definition.transferMode = SaveTransferMode;
			definition.downloadsPath = QFileInfo(path).canonicalPath();

			HandlersManager::setHandler(m_transfer->getMimeType().name(), definition);
		}
	}

	TransfersManager::addTransfer(m_transfer);

	accept();
}

void TransferDialog::setProgress(qint64 bytesReceived, qint64 bytesTotal)
{
	if (bytesTotal < 1)
	{
		m_ui->sizeTextLabelWidget->setText(tr("unknown"));
	}
	else if (bytesReceived > 0 && bytesReceived == bytesTotal)
	{
		m_ui->sizeTextLabelWidget->setText(tr("%1 (download completed)").arg(Utils::formatUnit(bytesTotal)));
	}
	else
	{
		const int progress((bytesReceived > 0 && bytesTotal > 0) ? qFloor((static_cast<qreal>(bytesReceived) / bytesTotal) * 100) : 0);

		m_ui->sizeTextLabelWidget->setText(tr("%1 (%2% downloaded)").arg(Utils::formatUnit(bytesTotal)).arg(progress));
	}
}

}
