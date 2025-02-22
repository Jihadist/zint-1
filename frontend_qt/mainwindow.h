/***************************************************************************
 *   Copyright (C) 2008 by BogDan Vatra <bogdan@licentia.eu>               *
 *   Copyright (C) 2009-2021 by Robin Stuart <rstuart114@gmail.com>        *
 *                                                                         *
 *   This program is free software: you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/
/* vim: set ts=4 sw=4 et : */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtGui>
#include <QGraphicsItem>
#include <QMainWindow>
#include <QGraphicsScene>
#include <QButtonGroup>

#include "ui_mainWindow.h"
#include "barcodeitem.h"

class QLabel;
class QShortcut;
class QDoubleSpinBox;
class QPushButton;

class MainWindow : public QWidget, private Ui::mainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = 0, Qt::WindowFlags fl = Qt::WindowFlags());
    ~MainWindow();

    static QString get_zint_version(void);

public slots:
    void update_preview();
    void change_options();
    void on_fgcolor_clicked();
    void on_bgcolor_clicked();
    void composite_ui_set();
    void composite_ean_check();
    void maxi_scm_ui_set();
    void msi_plessey_ui_set();
    void change_print_scale();
    void change_cmyk();
    void autoheight_ui_set();
    void HRTShow_ui_set();
    void dotty_ui_set();
    void codeone_ui_set();
    void structapp_ui_set();
    void on_encoded();
    void on_errored();
    void filter_symbologies();

    bool save();
    void about();
    void help();
    void quit_now();
    void menu();

    void reset_colours();
    void open_data_dialog();
    void open_sequence_dialog();
    void open_cli_dialog();

    void copy_to_clipboard_bmp();
    void copy_to_clipboard_emf();
    void copy_to_clipboard_eps();
    void copy_to_clipboard_gif();
#ifndef NO_PNG
    void copy_to_clipboard_png();
#endif
    void copy_to_clipboard_pcx();
    void copy_to_clipboard_svg();
    void copy_to_clipboard_tif();

    void copy_to_clipboard_errtxt();

    void height_per_row_disable();
    void height_per_row_default();

    void guard_reset_upcean();
    void guard_reset_upca();

    void view_context_menu(const QPoint &pos);
    void errtxtBar_context_menu(const QPoint &pos);

protected:
    void resizeEvent(QResizeEvent *event);
    bool event(QEvent *event) override;

    void combobox_item_enabled(QComboBox *comboBox, int index, bool enabled);
    void upcean_addon_gap(const QString &comboBoxName, const QString &labelName, int base);
    void upcean_guard_descent(const QString &spnBoxName, const QString &labelName);
    void guard_reset(const QString &spnBoxName);
    void set_gs1_mode(bool gs1_mode);
    void set_smaller_font(const QString &labelName);

    void createActions();
    void createMenu();
    void enableActions(bool enabled);

    void copy_to_clipboard(const QString &filename, const QString &name, const char *mimeType = nullptr);

    void errtxtBar_clear();
    void errtxtBar_set(bool isError);

    QPoint get_context_menu_pos(const QPoint &pos, QWidget *widget);

    QWidget *get_widget(const QString &name);

    static QString get_setting_name(int symbology);

    int get_rad_grp_index(const QStringList &names);
    void set_rad_from_setting(QSettings &settings, const QString &setting, const QStringList &names,
            int default_val = 0);
    bool get_rad_val(const QString &name);

    int get_cmb_index(const QString &name);
    void set_cmb_from_setting(QSettings &settings, const QString &setting, const QString &name, int default_val = 0);

    int get_chk_val(const QString &name);
    void set_chk_from_setting(QSettings &settings, const QString &setting, const QString &name, int default_val = 0);

    double get_dspn_val(const QString &name);
    void set_dspn_from_setting(QSettings &settings, const QString &setting, const QString &name,
            float default_val = 0.0f);

    QString get_txt_val(const QString &name);
    void set_txt_from_setting(QSettings &settings, const QString &setting, const QString &name,
            const QString &default_val);

    int get_spn_val(const QString &name);
    void set_spn_from_setting(QSettings &settings, const QString &setting, const QString &name, int default_val = 0);

    void save_sub_settings(QSettings &settings, int symbology);
    void load_sub_settings(QSettings &settings, int symbology);

private:
    QColor m_fgcolor,m_bgcolor;
    BarcodeItem m_bc;
    QWidget *m_optionWidget;
    QGraphicsScene *scene;
    int m_symbology;
    QShortcut *m_saveAsShortcut;
    QMenu *m_menu;
    QAction *m_copyBMPAct;
    QAction *m_copyEMFAct;
    QAction *m_copyEPSAct;
    QAction *m_copyGIFAct;
    QAction *m_copyPCXAct;
    QAction *m_copyPNGAct;
    QAction *m_copySVGAct;
    QAction *m_copyTIFAct;
    QAction *m_CLIAct;
    QAction *m_saveAsAct;
    QAction *m_aboutAct;
    QAction *m_helpAct;
    QAction *m_quitAct;
    QAction *m_copyErrtxtAct;
    QLabel *m_lblHeightPerRow;
    QDoubleSpinBox *m_spnHeightPerRow;
    QPushButton *m_btnHeightPerRowDisable;
    QPushButton *m_btnHeightPerRowDefault;
};

#endif
