/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2018 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*
**************************************************************************/

#include "LinksContentsWidget.h"
#include "../../../core/Application.h"
#include "../../../core/ThemesManager.h"
#include "../../../ui/Action.h"
#include "../../../ui/MainWindow.h"
#include "../../../ui/Window.h"

#include "ui_LinksContentsWidget.h"

#include <QtGui/QClipboard>
#include <QtWidgets/QDesktopWidget>
#include <QtWidgets/QToolTip>

namespace Otter
{

LinksContentsWidget::LinksContentsWidget(const QVariantMap &parameters, QWidget *parent) : ContentsWidget(parameters, nullptr, parent),
	m_window(nullptr),
	m_ui(new Ui::LinksContentsWidget)
{
	m_ui->setupUi(this);
	m_ui->filterLineEditWidget->setClearOnEscape(true);
	m_ui->linksViewWidget->setViewMode(ItemViewWidget::TreeViewMode);
	m_ui->linksViewWidget->setModel(new QStandardItemModel(this));
	m_ui->linksViewWidget->viewport()->installEventFilter(this);
	m_ui->linksViewWidget->viewport()->setMouseTracking(true);

	const MainWindow *mainWindow(MainWindow::findMainWindow(parentWidget()));

	if (mainWindow)
	{
		m_window = mainWindow->getActiveWindow();

		connect(mainWindow, &MainWindow::currentWindowChanged, this, [=]()
		{
			Window *window(mainWindow->getActiveWindow());

			if (window != m_window)
			{
				if (m_window)
				{
					disconnect(m_window, &Window::loadingStateChanged, this, &LinksContentsWidget::updateLinks);
				}

				m_window = window;

				if (window)
				{
					connect(window, &Window::loadingStateChanged, this, &LinksContentsWidget::updateLinks);
				}
			}

			updateLinks();
		});
	}

	updateLinks();

	connect(m_ui->filterLineEditWidget, &LineEditWidget::textChanged, m_ui->linksViewWidget, &ItemViewWidget::setFilterString);
	connect(m_ui->linksViewWidget, &ItemViewWidget::customContextMenuRequested, this, &LinksContentsWidget::showContextMenu);
	connect(m_ui->linksViewWidget, &ItemViewWidget::clicked, [&](const QModelIndex &index)
	{
		Application::triggerAction(ActionsManager::OpenUrlAction, {{QLatin1String("url"), index.data(Qt::StatusTipRole)}}, parentWidget());
	});
	connect(m_ui->linksViewWidget, &ItemViewWidget::needsActionsUpdate, this, [&]()
	{
		emit arbitraryActionsStateChanged({ActionsManager::CopyLinkToClipboardAction});
	});
}

LinksContentsWidget::~LinksContentsWidget()
{
	delete m_ui;
}

void LinksContentsWidget::changeEvent(QEvent *event)
{
	ContentsWidget::changeEvent(event);

	if (event->type() == QEvent::LanguageChange)
	{
		m_ui->retranslateUi(this);
	}
}

void LinksContentsWidget::triggerAction(int identifier, const QVariantMap &parameters)
{
	if (identifier == ActionsManager::CopyLinkToClipboardAction)
	{
		const QList<QModelIndex> indexes(m_ui->linksViewWidget->selectionModel()->selectedIndexes());
		QStringList links;
		links.reserve(indexes.count());

		for (int i = 0; i < indexes.count(); ++i)
		{
			links.append(indexes.at(i).data(Qt::StatusTipRole).toString());
		}

		QGuiApplication::clipboard()->setText(links.join(QLatin1Char('\n')));
	}
	else
	{
		ContentsWidget::triggerAction(identifier, parameters);
	}
}

void LinksContentsWidget::updateLinks()
{
	m_ui->linksViewWidget->getSourceModel()->clear();

	if (m_window && m_window->getWebWidget())
	{
		QStandardItem *pageEntry(addEntry(nullptr, m_window->getTitle(), m_window->getUrl()));
		const QVector<WebWidget::LinkUrl> links(m_window->getWebWidget()->getLinks());

		for (int i = 0; i < links.count(); ++i)
		{
			addEntry(pageEntry, links.at(i).title, links.at(i).url);
		}

		m_ui->linksViewWidget->expandAll();
	}
}

void LinksContentsWidget::showContextMenu(const QPoint &position)
{
	const QModelIndex index(m_ui->linksViewWidget->indexAt(position));

	if (index.isValid())
	{
		QMenu menu(this);
		menu.addAction(new Action(ActionsManager::CopyLinkToClipboardAction, {}, ActionExecutor::Object(this, this), &menu));
		menu.exec(m_ui->linksViewWidget->mapToGlobal(position));
	}
}

QStandardItem* LinksContentsWidget::addEntry(QStandardItem *parent, const QString &title, const QUrl &url)
{
	QStandardItem *item(new QStandardItem(title.isEmpty() ? url.toDisplayString(QUrl::RemovePassword) : title));
	item->setData(url, Qt::StatusTipRole);

	if (parent)
	{
		parent->appendRow(item);
	}
	else
	{
		m_ui->linksViewWidget->getSourceModel()->appendRow(item);
	}

	return item;
}

QString LinksContentsWidget::getTitle() const
{
	return tr("Links");
}

QLatin1String LinksContentsWidget::getType() const
{
	return QLatin1String("links");
}

QUrl LinksContentsWidget::getUrl() const
{
	return {};
}

QIcon LinksContentsWidget::getIcon() const
{
	return ThemesManager::createIcon(QLatin1String("links"), false);
}

ActionsManager::ActionDefinition::State LinksContentsWidget::getActionState(int identifier, const QVariantMap &parameters) const
{
	if (identifier == ActionsManager::CopyLinkToClipboardAction)
	{
		ActionsManager::ActionDefinition::State state(ActionsManager::getActionDefinition(ActionsManager::CopyLinkToClipboardAction).getDefaultState());
		state.isEnabled = !m_ui->linksViewWidget->selectionModel()->selectedIndexes().isEmpty();

		return state;
	}

	return ContentsWidget::getActionState(identifier, parameters);
}

bool LinksContentsWidget::eventFilter(QObject *object, QEvent *event)
{
	if (object == m_ui->linksViewWidget->viewport() && event->type() == QEvent::ToolTip)
	{
		const QHelpEvent *helpEvent(static_cast<QHelpEvent*>(event));
		const QModelIndex index(m_ui->linksViewWidget->indexAt(helpEvent->pos()));
		QString toolTip;

		if (index.data(Qt::DisplayRole).toString() != index.data(Qt::StatusTipRole).toUrl().toDisplayString(QUrl::RemovePassword))
		{
			toolTip.append(tr("Title: %1").arg(index.data(Qt::DisplayRole).toString()) + QLatin1Char('\n'));
		}

		toolTip.append(tr("Address: %1").arg(index.data(Qt::StatusTipRole).toUrl().toDisplayString()));

		QToolTip::showText(helpEvent->globalPos(), QFontMetrics(QToolTip::font()).elidedText(toolTip, Qt::ElideRight, (QApplication::desktop()->screenGeometry(m_ui->linksViewWidget).width() / 2)), m_ui->linksViewWidget, m_ui->linksViewWidget->visualRect(index));

		return true;
	}

	return ContentsWidget::eventFilter(object, event);
}

}
