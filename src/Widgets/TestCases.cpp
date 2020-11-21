/*
 * Copyright (C) 2019-2020 Ashar Khan <ashar786khan@gmail.com>
 *
 * This file is part of CP Editor.
 *
 * CP Editor is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * I will not be responsible if CP Editor behaves in unexpected way and
 * causes your ratings to go down and or lose any important contest.
 *
 * Believe Software is "Software" and it isn't immune to bugs.
 *
 */

#include "Widgets/TestCases.hpp"
#include "Core/EventLogger.hpp"
#include "Core/MessageLogger.hpp"
#include "Settings/DefaultPathManager.hpp"
#include "Util/FileUtil.hpp"
#include "Widgets/TestCase.hpp"
#include "generated/SettingsHelper.hpp"
#include <QComboBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QVBoxLayout>

#define VALIDATE_INDEX(x) validateIndex(x, __func__)

namespace Widgets
{
const int TestCases::MAX_NUMBER_OF_TESTCASES;

TestCases::TestCases(MessageLogger *logger, QWidget *parent) : QWidget(parent), log(logger)
{
    mainLayout = new QVBoxLayout(this);
    titleLayout = new QHBoxLayout();
    checkerLayout = new QHBoxLayout();
    label = new QLabel(tr("Test Cases"));
    verdicts = new QLabel();
    checkerLabel = new QLabel(tr("Checker:"));
    addButton = new QPushButton(tr("Add Test"));
    moreButton = new QPushButton(tr("More"));
    addCheckerButton = new QPushButton(tr("Add Checker"));
    checkerComboBox = new QComboBox();
    scrollArea = new QScrollArea();
    scrollAreaWidget = new QWidget();
    scrollAreaLayout = new QVBoxLayout(scrollAreaWidget);

    titleLayout->addWidget(label);
    titleLayout->addWidget(verdicts);
    titleLayout->addWidget(addButton);
    titleLayout->addWidget(moreButton);
    checkerLayout->addWidget(checkerLabel);
    checkerLayout->addWidget(checkerComboBox);
    checkerLayout->addWidget(addCheckerButton);
    scrollArea->setWidgetResizable(true);
    scrollArea->setWidget(scrollAreaWidget);
    mainLayout->addLayout(titleLayout);
    mainLayout->addLayout(checkerLayout);
    mainLayout->addWidget(scrollArea);

    verdicts->setToolTip(tr("Unaccepted / Accepted / Total"));
    addCheckerButton->setToolTip(tr("Add a custom testlib checker"));

    updateVerdicts();

    moreMenu = new QMenu();

    moreMenu->addAction(tr("Add Pairs of Testcases From Files"), [this] {
        QStringList paths =
            DefaultPathManager::getOpenFileNames("Add Pairs Of Test Cases", this, tr("Choose Testcase Files"));
        LOG_INFO(paths.join(", "));
        if (!paths.isEmpty())
        {
            QVariantList rules = SettingsHelper::getTestcasesMatchingRules();
            QSet<QString> remain;
            for (auto path : paths)
                remain.insert(QFileInfo(path).fileName());
            // load pairs
            for (auto rule : rules)
            {
                QRegularExpression inputRegex("^" + rule.toStringList().front() + "$");
                QString answerReplace(rule.toStringList().back());
                for (auto path : paths)
                {
                    auto inputFile = QFileInfo(path).fileName();
                    if (!remain.contains(inputFile))
                        continue;
                    if (!inputRegex.match(inputFile).hasMatch())
                        continue;
                    auto answerFile = inputFile;
                    answerFile.replace(inputRegex, answerReplace);
                    if (!remain.contains(answerFile))
                        continue;
                    remain.remove(inputFile);
                    remain.remove(answerFile);
                    auto answerPath = QFileInfo(path).dir().filePath(answerFile);
                    auto input = loadTestCaseFromFile(path, tr("Testcases"));
                    auto answer = loadTestCaseFromFile(answerPath, tr("Testcases"));
                    if (!input.isNull() && !answer.isNull())
                    {
                        addTestCase(input, answer);
                        log->info(tr("Load Testcases"),
                                  tr("A pair of testcases [%1] and [%2] is loaded").arg(path).arg(answerPath));
                    }
                }
            }
            // load single input
            for (auto rule : rules)
            {
                QRegularExpression inputRegex("^" + rule.toStringList().front() + "$");
                for (auto path : paths)
                {
                    auto inputFile = QFileInfo(path).fileName();
                    if (!remain.contains(inputFile))
                        continue;
                    if (!inputRegex.match(inputFile).hasMatch())
                        continue;
                    remain.remove(inputFile);
                    auto input = loadTestCaseFromFile(path, tr("Testcases"));
                    if (!input.isNull())
                    {
                        addTestCase(input, QString());
                        log->info(tr("Load Testcases"), tr("An input [%1] is loaded").arg(path));
                    }
                }
            }
            if (!remain.isEmpty())
            {
                QStringList remainPaths;
                for (auto path : remain)
                    remainPaths.push_back(QString("[%1]").arg(path));
                log->warn(tr("Load Testcases"),
                          tr("The following files are not loaded because they are not matched:%1. You can set the "
                             "matching rules at %2.")
                              .arg(remainPaths.join(", "))
                              .arg(SettingsHelper::pathOfTestcasesMatchingRules()),
                          false);
            }
        }
    });

    moreMenu->addAction(tr("Remove Empty"), [this] {
        LOG_INFO("Testcases Removing empty");
        for (int i = 0; i < count(); ++i)
        {
            if (input(i).isEmpty() && output(i).isEmpty() && expected(i).isEmpty())
            {
                onChildDeleted(testcases[i]);
                --i;
            }
        }
    });

    moreMenu->addAction(tr("Remove All"), [this] {
        LOG_INFO("Testcases removing all testcases");
        auto res =
            QMessageBox::question(this, tr("Clear Testcases"), tr("Are you sure you want to delete all test cases?"));
        if (res == QMessageBox::Yes)
        {
            for (int i = 0; i < count(); ++i)
            {
                onChildDeleted(testcases[i]);
                --i;
            }
        }
    });

    moreMenu->addAction(tr("Hide AC"), [this] {
        LOG_INFO("Testcases hiding all Accepted");
        for (auto t : testcases)
            if (t->verdict() == TestCase::AC)
                t->setShow(false);
    });

    moreMenu->addAction(tr("Show All"), [this] {
        LOG_INFO("Testcases making all cases visible");
        for (auto t : testcases)
            t->setShow(true);
    });

    moreMenu->addAction(tr("Hide All"), [this] {
        LOG_INFO("Testcases Hiding all cases");
        for (auto t : testcases)
            t->setShow(false);
    });

    moreMenu->addAction(tr("Invert"), [this] {
        LOG_INFO("Testcases Inverting all cases");
        for (auto t : testcases)
            t->setShow(t->isShow() ^ 1);
    });

    moreButton->setMenu(moreMenu);

    checkerLabel->setSizePolicy({QSizePolicy::Maximum, QSizePolicy::Fixed});
    checkerComboBox->setSizePolicy({QSizePolicy::Expanding, QSizePolicy::Fixed});
    addCheckerButton->setSizePolicy({QSizePolicy::Maximum, QSizePolicy::Fixed});
    checkerComboBox->setMinimumWidth(100);

    checkerComboBox->addItems({tr("Ignore trailing spaces"), tr("Strictly the same"), tr("ncmp - Compare int64s"),
                               tr("rcmp4 - Compare doubles, max error 1e-4"),
                               tr("rcmp6 - Compare doubles, max error 1e-6"),
                               tr("rcmp9 - Compare doubles, max error 1e-9"), tr("wcmp - Compare tokens"),
                               tr("nyesno - Compare YES/NOs, case insensitive")});
    checkerComboBox->setCurrentIndex(0);

    connect(checkerComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, &TestCases::checkerChanged);
    connect(addButton, &QPushButton::clicked, this, &TestCases::on_addButton_clicked);
    connect(addCheckerButton, &QPushButton::clicked, this, &TestCases::on_addCheckerButton_clicked);
}

void TestCases::setInput(int index, const QString &input)
{
    if (VALIDATE_INDEX(index))
        testcases[index]->setInput(input);
}

void TestCases::setOutput(int index, const QString &output)
{
    if (VALIDATE_INDEX(index))
        testcases[index]->setOutput(output);
}

void TestCases::setExpected(int index, const QString &expected)
{
    if (VALIDATE_INDEX(index))
        testcases[index]->setExpected(expected);
}

void TestCases::addTestCase(const QString &input, const QString &expected)
{
    if (count() >= MAX_NUMBER_OF_TESTCASES)
    {
        LOG_WARN("Max testcase limit reached");
        QMessageBox::warning(this, tr("Add Test Case"),
                             tr("There are already %1 test cases, you can't add more.").arg(count()));
    }
    else
    {
        LOG_INFO("New testcase added");
        auto testcase = new TestCase(count(), log, this, input, expected);
        connect(testcase, &TestCase::deleted, this, &TestCases::onChildDeleted);
        connect(testcase, &TestCase::requestRun, this, &TestCases::requestRun);
        testcases.push_back(testcase);
        scrollAreaLayout->addWidget(testcase);
        updateVerdicts();
    }
}

void TestCases::clearOutput()
{
    for (int i = 0; i < count(); ++i)
        testcases[i]->clearOutput();
    updateVerdicts();
}

void TestCases::clear()
{
    while (count() > 0)
        onChildDeleted(testcases.front());
}

QString TestCases::input(int index) const
{
    return VALIDATE_INDEX(index) ? testcases[index]->input() : QString();
}

QString TestCases::output(int index) const
{
    return VALIDATE_INDEX(index) ? testcases[index]->output() : QString();
}

QString TestCases::expected(int index) const
{
    return VALIDATE_INDEX(index) ? testcases[index]->expected() : QString();
}

void TestCases::loadStatus(const QStringList &inputList, const QStringList &expectedList)
{
    clear();
    for (int i = 0; i < inputList.length() && i < expectedList.length(); ++i)
        addTestCase(inputList[i], expectedList[i]);
}

QStringList TestCases::inputs() const
{
    QStringList res;
    for (int i = 0; i < count(); ++i)
        res.append(testcases[i]->input());
    return res;
}

QStringList TestCases::expecteds() const
{
    QStringList res;
    for (int i = 0; i < count(); ++i)
        res.append(testcases[i]->expected());
    return res;
}

void TestCases::loadFromSavedFiles(const QString &filePath)
{
    clear();

    for (int i = MAX_NUMBER_OF_TESTCASES - 1; i >= 0; --i)
    {
        auto inputPath = inputFilePath(filePath, i);
        auto answerPath = answerFilePath(filePath, i);
        if (QFile::exists(inputPath) || QFile::exists(answerPath))
        {
            for (int j = 0; j <= i; ++j)
            {
                addTestCase(loadTestCaseFromFile(inputFilePath(filePath, j), tr("Input #%1").arg(j + 1)),
                            loadTestCaseFromFile(answerFilePath(filePath, j), tr("Expected #%1").arg(j + 1)));
            }
            break;
        }
    }

    if (count() == 0)
        addTestCase();
}

void TestCases::saveToFiles(const QString &filePath, bool safe)
{
    for (int i = 0; i < count(); ++i)
    {
        if (!input(i).isEmpty())
            Util::saveFile(inputFilePath(filePath, i), input(i), tr("Save Input #%1").arg(i + 1), safe, log, true);
        if (!expected(i).isEmpty())
            Util::saveFile(answerFilePath(filePath, i), expected(i), tr("Save Expected #%1").arg(i + 1), safe, log,
                           true);
    }
    for (int i = count(); i < MAX_NUMBER_OF_TESTCASES; ++i)
    {
        auto inputPath = inputFilePath(filePath, i);
        auto answerPath = answerFilePath(filePath, i);
        if (QFile::exists(inputPath))
            QFile::remove(inputPath);
        if (QFile::exists(answerPath))
            QFile::remove(answerPath);
    }
}

QString TestCases::loadTestCaseFromFile(const QString &path, const QString &head)
{
    return Util::readFile(path, tr("Load %1").arg(head), log, true);
}

void TestCases::setTestCaseEditFont(const QFont &font)
{
    for (auto t : testcases)
        t->setTestCaseEditFont(font);
}

void TestCases::updateHeights()
{
    for (auto t : testcases)
        t->updateHeight();
}

QVariantList TestCases::splitterStates() const
{
    QVariantList states;
    for (auto t : testcases)
    {
        QVariantList tmp;
        for (auto size : t->splitterSizes())
            tmp.push_back(size);
        states.push_back(tmp);
    }
    return states;
}

void TestCases::restoreSplitterStates(const QVariantList &states)
{
    for (int i = 0; i < count() && i < states.count(); ++i)
    {
        QList<int> sizes;
        for (auto var : states[i].toList())
            sizes.push_back(var.toInt());
        testcases[i]->restoreSplitterSizes(sizes);
    }
}

int TestCases::id(TestCase *testcase) const
{
    return testcases.indexOf(testcase);
}

int TestCases::count() const
{
    return testcases.count();
}

void TestCases::setCheckerIndex(int index)
{
    if (index >= 0 && index < checkerComboBox->count())
        checkerComboBox->setCurrentIndex(index);
    else
        LOG_ERR("Invalid checker index " << index);
}

int TestCases::checkerIndex() const
{
    return checkerComboBox->currentIndex();
}

void TestCases::addCustomCheckers(const QStringList &list)
{
    checkerComboBox->addItems(list);
}

QStringList TestCases::customCheckers() const
{
    QStringList res;
    for (int i = 8; i < checkerComboBox->count(); ++i)
        res.push_back(checkerComboBox->itemText(i));
    return res;
}

QString TestCases::checkerText() const
{
    return checkerComboBox->currentText();
}

Core::Checker::CheckerType TestCases::checkerType() const
{
    switch (checkerIndex())
    {
    case 0:
        return Core::Checker::IgnoreTrailingSpaces;
    case 1:
        return Core::Checker::Strict;
    case 2:
        return Core::Checker::Ncmp;
    case 3:
        return Core::Checker::Rcmp4;
    case 4:
        return Core::Checker::Rcmp6;
    case 5:
        return Core::Checker::Rcmp9;
    case 6:
        return Core::Checker::Wcmp;
    case 7:
        return Core::Checker::Nyesno;
    default:
        return Core::Checker::Custom;
    }
}

void TestCases::setShow(int index, bool show)
{
    if (VALIDATE_INDEX(index))
        testcases[index]->setShow(show);
}

bool TestCases::isShow(int index) const
{
    return VALIDATE_INDEX(index) ? testcases[index]->isShow() : false;
}

void TestCases::setVerdict(int index, TestCase::Verdict verdict)
{
    if (VALIDATE_INDEX(index))
    {
        testcases[index]->setVerdict(verdict);
        updateVerdicts();
    }
}

void TestCases::on_addButton_clicked()
{
    addTestCase();
}

void TestCases::on_addCheckerButton_clicked()
{
    LOG_INFO("Add checker button clicked");
    auto path =
        QFileInfo(DefaultPathManager::getOpenFileName("Custom Checker", this, tr("Add Checker"))).canonicalFilePath();
    if (!path.isEmpty())
    {
        checkerComboBox->addItem(path);
        checkerComboBox->setCurrentIndex(checkerComboBox->count() - 1);
    }
}

void TestCases::onChildDeleted(TestCase *widget)
{
    testcases.removeOne(widget);
    widget->hide();
    scrollAreaLayout->removeWidget(widget);
    delete widget;
    for (int i = 0; i < count(); ++i)
        testcases[i]->setID(i);
    updateVerdicts();
}

bool TestCases::validateIndex(int index, const QString &funcName) const
{
    if (index >= 0 && index < count())
        return true;
    else
    {
        LOG_DEV(INFO_OF(index) << INFO_OF(count()) << INFO_OF(funcName));
        return false;
    }
}

void TestCases::updateVerdicts()
{
    int accepted = 0, unaccepted = 0;
    for (auto t : testcases)
    {
        switch (t->verdict())
        {
        case TestCase::AC:
            ++accepted;
            break;
        case TestCase::WA:
        case TestCase::TLE:
        case TestCase::RE:
            ++unaccepted;
            break;
        case TestCase::UNKNOWN:
            break;
        default:
            Q_UNREACHABLE();
            break;
        }
    }
    verdicts->setText(QString("<span style=\"color:red\">%1</span> / <span style=\"color:green\">%2</span> / %3")
                          .arg(unaccepted)
                          .arg(accepted)
                          .arg(count()));
}

QString TestCases::inputFilePath(const QString &filePath, int index)
{
    return testCaseFilePath(SettingsHelper::getInputFileSavePath(), filePath, index);
}

QString TestCases::answerFilePath(const QString &filePath, int index)
{
    return testCaseFilePath(SettingsHelper::getAnswerFileSavePath(), filePath, index);
}

QString TestCases::testCaseFilePath(QString rule, const QString &filePath, int index)
{
    QFileInfo fileInfo(filePath);
    return fileInfo.dir().filePath(rule.replace("${filename}", fileInfo.fileName())
                                       .replace("${basename}", fileInfo.completeBaseName())
                                       .replace("${0-index}", QString::number(index))
                                       .replace("${1-index}", QString::number(index + 1)));
}
} // namespace Widgets
