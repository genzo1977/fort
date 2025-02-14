#include "optionspage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QToolButton>
#include <QVBoxLayout>

#include <conf/confmanager.h>
#include <conf/firewallconf.h>
#include <form/controls/controlutil.h>
#include <form/opt/optionscontroller.h>
#include <fortmanager.h>
#include <fortsettings.h>
#include <manager/translationmanager.h>
#include <manager/windowmanager.h>
#include <user/iniuser.h>
#include <util/guiutil.h>
#include <util/iconcache.h>
#include <util/startuputil.h>

namespace {

struct Startup
{
    quint8 initialized : 1 = false;
    quint8 isServiceChanged : 1 = false;
    quint8 wasService : 1 = false;
    quint8 isService : 1 = false;
} g_startup;

void updateComboBox(
        QComboBox *c, const QStringList &names, const QStringList &iconPaths, int currentIndex)
{
    int index = 0;
    for (const QString &name : names) {
        const QString &iconPath = iconPaths.at(index);

        c->setItemText(index, name);
        c->setItemIcon(index, IconCache::icon(iconPath));

        ++index;
    }

    c->setCurrentIndex(currentIndex);
}

}

OptionsPage::OptionsPage(OptionsController *ctrl, QWidget *parent) : OptBasePage(ctrl, parent)
{
    setupStartup();
    setupUi();
}

void OptionsPage::onResetToDefault()
{
    m_cbFilterEnabled->setChecked(true);
    m_comboBlockTraffic->setCurrentIndex(0);
    m_comboFilterMode->setCurrentIndex(0);

    m_cbBootFilter->setChecked(false);
    m_cbNoServiceControl->setChecked(false);
    m_cbCheckPasswordOnUninstall->setChecked(false);
    m_cbPassword->setChecked(false);

    m_cbUpdateKeepCurrentVersion->setChecked(false);
    m_cbUpdateAutoDownload->setChecked(false);

    m_cbLogBlocked->setChecked(true);
    m_cbPurgeOnMounted->setChecked(false);

    m_cbFilterLocals->setChecked(false);
    m_cbFilterLocalNet->setChecked(false);

    m_cbLogDebug->setChecked(false);
    m_cbLogConsole->setChecked(false);
}

void OptionsPage::onAboutToSave()
{
    // Startup
    saveAutoRunMode(m_comboAutoRun->currentIndex());
    saveService(m_cbService->isChecked());

    // Password
    if (passwordEdited()) {
        const bool isPasswordCleared = (ini()->hasPassword() && ini()->password().isEmpty());
        if (isPasswordCleared && !settings()->hasPassword()) {
            m_cbPassword->setChecked(false);
        }

        if (!ini()->hasPassword()) {
            settings()->resetCheckedPassword();
        }
    } else if (conf()->iniEdited()) {
        ini()->setHasPassword(settings()->hasPassword());
    }
}

void OptionsPage::onEditResetted()
{
    // Password
    setPasswordEdited(false);
    retranslateEditPassword();
}

void OptionsPage::saveAutoRunMode(int mode)
{
    if (m_currentAutoRunMode == mode)
        return;

    m_currentAutoRunMode = mode;

    StartupUtil::setAutoRunMode(mode, settings()->defaultLanguage());
}

void OptionsPage::saveService(bool isService)
{
    if (g_startup.isService == isService)
        return;

    g_startup.isService = isService;

    if (!g_startup.isServiceChanged) {
        g_startup.isServiceChanged = true;

        connect(fortManager(), &FortManager::aboutToDestroy, [=] {
            if (g_startup.wasService != g_startup.isService) {
                StartupUtil::setServiceInstalled(g_startup.isService);
            }
        });
    }

    windowManager()->processRestartRequired(tr("Windows Service installation changed.") + '\n'
            + tr("The change will be applied only on program exit."));
}

void OptionsPage::onRetranslateUi()
{
    m_gbStartup->setTitle(tr("Startup"));
    m_gbTraffic->setTitle(tr("Traffic"));
    m_gbProtection->setTitle(tr("Self Protection"));
    m_gbUpdate->setTitle(tr("Auto Update"));
    m_gbProg->setTitle(tr("Programs"));
    m_gbLan->setTitle(tr("Local Area Network"));
    m_gbLogs->setTitle(tr("Logs"));

    m_labelStartMode->setText(tr("Auto-run:"));
    retranslateComboStartMode();

    m_cbService->setText(tr("Windows Service"));
    m_cbService->setToolTip(tr("Run Fort Firewall as a Service in background"));

    m_cbFilterEnabled->setText(tr("Filter Enabled"));

    m_labelBlockTraffic->setText(tr("Block Traffic:"));
    retranslateComboBlockTraffic();

    m_labelFilterMode->setText(tr("Filter Mode:"));
    retranslateComboFilterMode();

    m_cbBootFilter->setText(tr("Block traffic when Fort Firewall is not running"));
    m_cbNoServiceControl->setText(tr("Disable Service controls"));
    m_cbCheckPasswordOnUninstall->setText(tr("Check password on Uninstall"));

    m_cbPassword->setText(tr("Password:"));
    retranslateEditPassword();

    m_btPasswordLock->setText(tr("Lock the password (unlocked till \"%1\")")
                                      .arg(settings()->passwordUnlockedTillText()));

    m_cbUpdateKeepCurrentVersion->setText(tr("Keep current version"));
    m_cbUpdateAutoDownload->setText(tr("Auto-download new version"));

    m_cbLogBlocked->setText(tr("Collect New Programs"));
    m_cbPurgeOnMounted->setText(tr("Purge Obsolete only on mounted drives"));

    m_cbFilterLocals->setText(tr("Filter Local Addresses") + " (127.0.0.0/8, 255.255.255.255)");
    m_cbFilterLocals->setToolTip(
            tr("Filter Local Loopback (127.0.0.0/8) and Broadcast (255.255.255.255) Addresses"));
    m_cbFilterLocalNet->setText(tr("Filter Local Network"));

    m_cbLogDebug->setText(tr("Log debug messages"));
    m_cbLogConsole->setText(tr("Show log messages in console"));
}

void OptionsPage::retranslateComboStartMode()
{
    const QStringList list = { tr("Disabled"), tr("For current user"), tr("For all users") };

    int currentIndex = m_comboAutoRun->currentIndex();
    if (currentIndex < 0) {
        currentIndex = m_currentAutoRunMode;
    }

    ControlUtil::setComboBoxTexts(m_comboAutoRun, list, currentIndex);

    // Disable some items if user is not an administrator
    if (settings()->isUserAdmin())
        return;

    m_cbService->setEnabled(false);

    if (currentIndex >= StartupUtil::StartupAllUsers) {
        m_comboAutoRun->setEnabled(false);
        return;
    }

    auto comboModel = qobject_cast<QStandardItemModel *>(m_comboAutoRun->model());
    if (!comboModel)
        return;

    const int itemCount = comboModel->rowCount();
    for (int i = StartupUtil::StartupAllUsers; i < itemCount; ++i) {
        auto item = comboModel->item(i);
        if (item) {
            item->setEnabled(false);
        }
    }
}

void OptionsPage::retranslateComboBlockTraffic()
{
    updateComboBox(m_comboBlockTraffic, FirewallConf::blockTrafficNames(),
            FirewallConf::blockTrafficIconPaths(), conf()->blockTrafficIndex());
}

void OptionsPage::retranslateComboFilterMode()
{
    updateComboBox(m_comboFilterMode, FirewallConf::filterModeNames(),
            FirewallConf::filterModeIconPaths(), conf()->filterModeIndex());
}

void OptionsPage::retranslateEditPassword()
{
    m_editPassword->setPlaceholderText(
            settings()->hasPassword() ? tr("Installed") : tr("Not Installed"));
}

void OptionsPage::setupStartup()
{
    m_currentAutoRunMode = StartupUtil::autoRunMode();

    if (g_startup.initialized)
        return;

    g_startup.initialized = true;
    g_startup.wasService = g_startup.isService = settings()->hasService();
}

void OptionsPage::setupUi()
{
    // Column #1
    auto colLayout1 = setupColumn1();

    // Column #2
    auto colLayout2 = setupColumn2();

    // Main layout
    auto layout = new QHBoxLayout();
    layout->addLayout(colLayout1);
    layout->addStretch();
    layout->addLayout(colLayout2);
    layout->addStretch();

    this->setLayout(layout);
}

QLayout *OptionsPage::setupColumn1()
{
    // Startup Group Box
    setupStartupBox();

    // Traffic Group Box
    setupTrafficBox();

    // Protection Group Box
    setupProtectionBox();

    // Update Group Box
    setupUpdateBox();

    auto layout = new QVBoxLayout();
    layout->setSpacing(10);
    layout->addWidget(m_gbStartup);
    layout->addWidget(m_gbTraffic);
    layout->addWidget(m_gbProtection);
    layout->addWidget(m_gbUpdate);
    layout->addStretch();

    return layout;
}

QLayout *OptionsPage::setupColumn2()
{
    // Programs Group Box
    setupProgBox();

    // LAN Group Box
    setupLanBox();

    // Logs Group Box
    setupLogsBox();

    auto layout = new QVBoxLayout();
    layout->setSpacing(10);
    layout->addWidget(m_gbProg);
    layout->addWidget(m_gbLan);
    layout->addWidget(m_gbLogs);
    layout->addStretch();

    return layout;
}

void OptionsPage::setupStartupBox()
{
    auto startModeLayout = setupStartModeLayout();

    m_cbService = ControlUtil::createCheckBox(
            g_startup.isService, [&](bool /*checked*/) { ctrl()->emitEdited(); });

    auto layout = new QVBoxLayout();
    layout->addLayout(startModeLayout);
    layout->addWidget(m_cbService);

    m_gbStartup = new QGroupBox();
    m_gbStartup->setLayout(layout);
}

QLayout *OptionsPage::setupStartModeLayout()
{
    m_labelStartMode = ControlUtil::createLabel();

    m_comboAutoRun = ControlUtil::createComboBox(QStringList(), [&](int index) {
        if (m_currentAutoRunMode != index) {
            ctrl()->emitEdited();
        }
    });
    m_comboAutoRun->setFixedWidth(200);

    return ControlUtil::createRowLayout(m_labelStartMode, m_comboAutoRun);
}

void OptionsPage::setupTrafficBox()
{
    m_cbFilterEnabled = ControlUtil::createCheckBox(conf()->filterEnabled(), [&](bool checked) {
        conf()->setFilterEnabled(checked);
        ctrl()->setFlagsEdited();
    });

    auto blockTrafficLayout = setupBlockTrafficLayout();

    auto filterModeLayout = setupFilterModeLayout();

    auto layout = new QVBoxLayout();
    layout->addWidget(m_cbFilterEnabled);
    layout->addLayout(blockTrafficLayout);
    layout->addLayout(filterModeLayout);

    m_gbTraffic = new QGroupBox();
    m_gbTraffic->setLayout(layout);
}

QLayout *OptionsPage::setupBlockTrafficLayout()
{
    m_labelBlockTraffic = ControlUtil::createLabel();

    m_comboBlockTraffic =
            ControlUtil::createComboBox(FirewallConf::blockTrafficNames(), [&](int index) {
                if (conf()->blockTrafficIndex() != index) {
                    conf()->setBlockTrafficIndex(index);
                    ctrl()->setFlagsEdited();
                }
            });
    m_comboBlockTraffic->setFixedWidth(200);

    return ControlUtil::createRowLayout(m_labelBlockTraffic, m_comboBlockTraffic);
}

QLayout *OptionsPage::setupFilterModeLayout()
{
    m_labelFilterMode = ControlUtil::createLabel();

    m_comboFilterMode =
            ControlUtil::createComboBox(FirewallConf::filterModeNames(), [&](int index) {
                if (conf()->filterModeIndex() != index) {
                    conf()->setFilterModeIndex(index);
                    ctrl()->setFlagsEdited();
                }
            });
    m_comboFilterMode->setFixedWidth(200);

    // TODO: Implement Ask to Connect
    QStandardItemModel *model = qobject_cast<QStandardItemModel *>(m_comboFilterMode->model());
    QStandardItem *item = model->item(1);
    item->setFlags(item->flags() & ~Qt::ItemIsEnabled);

    return ControlUtil::createRowLayout(m_labelFilterMode, m_comboFilterMode);
}

void OptionsPage::setupProtectionBox()
{
    m_cbBootFilter = ControlUtil::createCheckBox(conf()->bootFilter(), [&](bool checked) {
        conf()->setBootFilter(checked);
        ctrl()->setFlagsEdited();
    });

    m_cbNoServiceControl =
            ControlUtil::createCheckBox(ini()->noServiceControl(), [&](bool checked) {
                ini()->setNoServiceControl(checked);
                ctrl()->setIniEdited();
            });

    m_cbCheckPasswordOnUninstall =
            ControlUtil::createCheckBox(ini()->checkPasswordOnUninstall(), [&](bool checked) {
                ini()->setCheckPasswordOnUninstall(checked);
                ctrl()->setIniEdited();
            });

    if (!settings()->hasMasterAdmin()) {
        m_cbCheckPasswordOnUninstall->setEnabled(false);
    }

    // Password Row
    auto passwordLayout = setupPasswordLayout();
    setupPasswordLock();

    auto layout = new QVBoxLayout();
    layout->addWidget(m_cbBootFilter);
    layout->addWidget(m_cbNoServiceControl);
    layout->addWidget(ControlUtil::createSeparator());
    layout->addWidget(m_cbCheckPasswordOnUninstall);
    layout->addLayout(passwordLayout);
    layout->addWidget(m_btPasswordLock, 0, Qt::AlignCenter);

    m_gbProtection = new QGroupBox();
    m_gbProtection->setLayout(layout);
}

QLayout *OptionsPage::setupPasswordLayout()
{
    auto layout = new QHBoxLayout();
    layout->setSpacing(6);

    m_cbPassword = ControlUtil::createCheckBox(settings()->hasPassword(), [&](bool checked) {
        if (checked) {
            m_editPassword->setFocus();
        } else {
            m_editPassword->clear();
        }

        setPasswordEdited(true);
        ini()->setHasPassword(checked);
        ctrl()->setIniEdited();
    });

    setupEditPassword();

    layout->addWidget(m_cbPassword);
    layout->addWidget(m_editPassword);

    return layout;
}

void OptionsPage::setupEditPassword()
{
    m_editPassword = ControlUtil::createLineEdit(QString(), [&](const QString &text) {
        m_cbPassword->setChecked(!text.isEmpty());

        ini()->setPassword(text);
        ctrl()->setIniEdited();
    });
    m_editPassword->setClearButtonEnabled(true);
    m_editPassword->setEchoMode(QLineEdit::Password);
    m_editPassword->setMaxLength(32);
    m_editPassword->setFixedWidth(200);
}

void OptionsPage::setupPasswordLock()
{
    m_btPasswordLock = ControlUtil::createToolButton(":/icons/lock_open.png", [&] {
        settings()->resetCheckedPassword();
        m_btPasswordLock->hide();
    });

    const auto refreshPasswordLock = [&] {
        m_btPasswordLock->setVisible(settings()->hasPassword()
                && settings()->passwordUnlockType() > FortSettings::UnlockWindow);
    };

    refreshPasswordLock();

    connect(settings(), &FortSettings::passwordCheckedChanged, this, refreshPasswordLock);
}

void OptionsPage::setupUpdateBox()
{
    m_cbUpdateKeepCurrentVersion =
            ControlUtil::createCheckBox(ini()->updateKeepCurrentVersion(), [&](bool checked) {
                ini()->setUpdateKeepCurrentVersion(checked);
                ctrl()->setIniEdited();
            });

    m_cbUpdateAutoDownload =
            ControlUtil::createCheckBox(ini()->updateAutoDownload(), [&](bool checked) {
                ini()->setUpdateAutoDownload(checked);
                ctrl()->setIniEdited();
            });

    // Layout
    auto layout = ControlUtil::createVLayoutByWidgets(
            { m_cbUpdateKeepCurrentVersion, m_cbUpdateAutoDownload });

    m_gbUpdate = new QGroupBox();
    m_gbUpdate->setLayout(layout);
}

void OptionsPage::setupProgBox()
{
    setupLogBlocked();

    m_cbPurgeOnMounted =
            ControlUtil::createCheckBox(ini()->progPurgeOnMounted(), [&](bool checked) {
                if (ini()->progPurgeOnMounted() != checked) {
                    ini()->setProgPurgeOnMounted(checked);
                    ctrl()->setIniEdited();
                }
            });

    // Layout
    auto layout = ControlUtil::createVLayoutByWidgets(
            { m_cbLogBlocked, ControlUtil::createSeparator(), m_cbPurgeOnMounted });

    m_gbProg = new QGroupBox();
    m_gbProg->setLayout(layout);
}

void OptionsPage::setupLogBlocked()
{
    m_cbLogBlocked = ControlUtil::createCheckBox(conf()->logBlocked(), [&](bool checked) {
        if (conf()->logBlocked() != checked) {
            conf()->setLogBlocked(checked);
            ctrl()->setFlagsEdited();
        }
    });

    m_cbLogBlocked->setFont(GuiUtil::fontBold());
}

void OptionsPage::setupLanBox()
{
    m_cbFilterLocals = ControlUtil::createCheckBox(conf()->filterLocals(), [&](bool checked) {
        conf()->setFilterLocals(checked);
        ctrl()->setFlagsEdited();
    });

    m_cbFilterLocalNet = ControlUtil::createCheckBox(conf()->filterLocalNet(), [&](bool checked) {
        conf()->setFilterLocalNet(checked);
        ctrl()->setFlagsEdited();
    });

    // Layout
    auto layout = ControlUtil::createVLayoutByWidgets({ m_cbFilterLocals, m_cbFilterLocalNet });

    m_gbLan = new QGroupBox();
    m_gbLan->setLayout(layout);
}

void OptionsPage::setupLogsBox()
{
    m_cbLogDebug = ControlUtil::createCheckBox(ini()->logDebug(), [&](bool checked) {
        ini()->setLogDebug(checked);
        ctrl()->setIniEdited();
    });
    m_cbLogConsole = ControlUtil::createCheckBox(ini()->logConsole(), [&](bool checked) {
        ini()->setLogConsole(checked);
        ctrl()->setIniEdited();
    });

    auto layout = new QVBoxLayout();
    layout->addWidget(m_cbLogDebug);
    layout->addWidget(m_cbLogConsole);

    m_gbLogs = new QGroupBox();
    m_gbLogs->setLayout(layout);
}
