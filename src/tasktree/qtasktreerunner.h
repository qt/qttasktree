// Copyright (C) 2025 Jarek Kobus
// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:significant reason:default


#ifndef QTASKTREERUNNER_H
#define QTASKTREERUNNER_H

#include <QtTaskTree/qttasktreeglobal.h>

#include <QtTaskTree/qtasktree.h>

#include <QtCore/QObject>

#include <unordered_map>

QT_BEGIN_NAMESPACE

namespace QtTaskTree {

class QAbstractTaskTreeRunnerPrivate;
class QSingleTaskTreeRunnerPrivate;
class QSequentialTaskTreeRunnerPrivate;
class QParallelTaskTreeRunnerPrivate;

class Q_TASKTREE_EXPORT QAbstractTaskTreeRunner : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QAbstractTaskTreeRunner)

public:
    using TreeSetupHandler = std::function<void(QTaskTree &)>;
    using TreeDoneHandler = std::function<void(const QTaskTree &, DoneWith)>;

    QAbstractTaskTreeRunner() : QAbstractTaskTreeRunner(nullptr) {}
    explicit QAbstractTaskTreeRunner(QObject *parent);
    ~QAbstractTaskTreeRunner() override;

    virtual bool isRunning() const = 0;
    virtual void cancel() = 0;
    virtual void reset() = 0;

Q_SIGNALS:
    void aboutToStart(QTaskTree *taskTree);
    void done(QtTaskTree::DoneWith result, QTaskTree *taskTree);

protected:
    bool event(QEvent *event) override;
    QAbstractTaskTreeRunner(QAbstractTaskTreeRunnerPrivate &dd, QObject *parent);

    template <typename Handler>
    static TreeSetupHandler wrapTreeSetupHandler(Handler &&handler) {
        if constexpr (std::is_same_v<std::decay_t<Handler>, TreeSetupHandler>) {
            if (!handler)
                return {}; // User passed {} for the setup handler.
        }
        // V, T stands for: [V]oid, [T]askTree
        static constexpr bool isVT = isInvocable<void, Handler, QTaskTree &>();
        static constexpr bool isV = isInvocable<void, Handler>();
        static_assert(isVT || isV,
            "Tree setup handler needs to take (TaskTree &) or (void) as an argument and has to "
            "return void. The passed handler doesn't fulfill these requirements.");
        return [handler = std::forward<Handler>(handler)](QTaskTree &taskTree) {
            if constexpr (isVT)
                std::invoke(handler, taskTree);
            else if constexpr (isV)
                std::invoke(handler);
        };
    }

    template <typename Handler>
    static TreeDoneHandler wrapTreeDoneHandler(Handler &&handler) {
        if constexpr (std::is_same_v<std::decay_t<Handler>, TreeDoneHandler>) {
            if (!handler)
                return {}; // User passed {} for the done handler.
        }
        // V, T, D stands for: [V]oid, [T]askTree, [D]oneWith
        static constexpr bool isVTD = isInvocable<void, Handler, const QTaskTree &, DoneWith>();
        static constexpr bool isVT = isInvocable<void, Handler, const QTaskTree &>();
        static constexpr bool isVD = isInvocable<void, Handler, DoneWith>();
        static constexpr bool isV = isInvocable<void, Handler>();
        static_assert(isVTD || isVT || isVD || isV,
            "Task done handler needs to take (const TaskTree &, DoneWith), (const Task &), "
            "(DoneWith) or (void) as arguments and has to return void. "
            "The passed handler doesn't fulfill these requirements.");
        return [handler = std::forward<Handler>(handler)](const QTaskTree &taskTree, DoneWith result) {
            if constexpr (isVTD)
                std::invoke(handler, taskTree, result);
            else if constexpr (isVT)
                std::invoke(handler, taskTree);
            else if constexpr (isVD)
                std::invoke(handler, result);
            else if constexpr (isV)
                std::invoke(handler);
        };
    }
};

class Q_TASKTREE_EXPORT QSingleTaskTreeRunner : public QAbstractTaskTreeRunner
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QSingleTaskTreeRunner)

public:
    QSingleTaskTreeRunner() : QSingleTaskTreeRunner(nullptr) {}
    explicit QSingleTaskTreeRunner(QObject *parent);
    ~QSingleTaskTreeRunner() override;

    bool isRunning() const override;
    void cancel() override;
    void reset() override;

    template <typename SetupHandler = TreeSetupHandler, typename DoneHandler = TreeDoneHandler>
    void start(const Group &recipe,
               SetupHandler &&setupHandler = {},
               DoneHandler &&doneHandler = {},
               CallDone callDone = CallDoneFlag::Always)
    {
        startImpl(recipe,
                  wrapTreeSetupHandler(std::forward<SetupHandler>(setupHandler)),
                  wrapTreeDoneHandler(std::forward<DoneHandler>(doneHandler)),
                  callDone);
    }

protected:
    bool event(QEvent *event) override;

private:
    void startImpl(const Group &recipe,
                   const TreeSetupHandler &setupHandler,
                   const TreeDoneHandler &doneHandler,
                   CallDone callDone);
};

class Q_TASKTREE_EXPORT QSequentialTaskTreeRunner : public QAbstractTaskTreeRunner
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QSequentialTaskTreeRunner)

public:
    QSequentialTaskTreeRunner() : QSequentialTaskTreeRunner(nullptr) {}
    explicit QSequentialTaskTreeRunner(QObject *parent);
    ~QSequentialTaskTreeRunner();

    bool isRunning() const override;
    void cancel() override;
    void reset() override;

    void cancelCurrent();
    void resetCurrent();

    template <typename SetupHandler = TreeSetupHandler, typename DoneHandler = TreeDoneHandler>
    void enqueue(const Group &recipe,
                 SetupHandler &&setupHandler = {},
                 DoneHandler &&doneHandler = {},
                 CallDone callDone = CallDoneFlag::Always)
    {
        enqueueImpl(recipe,
                    wrapTreeSetupHandler(std::forward<SetupHandler>(setupHandler)),
                    wrapTreeDoneHandler(std::forward<DoneHandler>(doneHandler)),
                    callDone);
    }

protected:
    bool event(QEvent *event) override;

private:
    void enqueueImpl(const Group &recipe,
                     const TreeSetupHandler &setupHandler,
                     const TreeDoneHandler &doneHandler,
                     CallDone callDone);
};

class Q_TASKTREE_EXPORT QParallelTaskTreeRunner : public QAbstractTaskTreeRunner
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QParallelTaskTreeRunner)

public:
    QParallelTaskTreeRunner() : QParallelTaskTreeRunner(nullptr) {}
    explicit QParallelTaskTreeRunner(QObject *parent);
    ~QParallelTaskTreeRunner();

    bool isRunning() const override;
    void cancel() override;
    void reset() override;

    template <typename SetupHandler = TreeSetupHandler, typename DoneHandler = TreeDoneHandler>
    void start(const Group &recipe,
               SetupHandler &&setupHandler = {},
               DoneHandler &&doneHandler = {},
               CallDone callDone = CallDoneFlag::Always)
    {
        startImpl(recipe,
                  wrapTreeSetupHandler(std::forward<SetupHandler>(setupHandler)),
                  wrapTreeDoneHandler(std::forward<DoneHandler>(doneHandler)),
                  callDone);
    }

protected:
    bool event(QEvent *event) override;

private:
    void startImpl(const Group &recipe,
                   const TreeSetupHandler &setupHandler,
                   const TreeDoneHandler &doneHandler,
                   CallDone callDone);
};

template <typename Key>
class QMappedTaskTreeRunner : public QAbstractTaskTreeRunner
{
public:
    QMappedTaskTreeRunner() : QMappedTaskTreeRunner(nullptr) {}
    explicit QMappedTaskTreeRunner(QObject *parent)
        : QAbstractTaskTreeRunner(parent)
    {}

    ~QMappedTaskTreeRunner() = default;

    bool isRunning() const override { return !m_taskTrees.empty(); }

    void cancel() override
    {
        while (!m_taskTrees.empty())
            m_taskTrees.begin()->second->cancel();
    }

    void reset() override { m_taskTrees.clear(); }

    bool isKeyRunning(const Key &key) const { return m_taskTrees.find(key) != m_taskTrees.end(); }

    void cancelKey(const Key &key)
    {
        if (const auto it = m_taskTrees.find(key); it != m_taskTrees.end())
            it->second->cancel();
    }

    void resetKey(const Key &key)
    {
        if (const auto it = m_taskTrees.find(key); it != m_taskTrees.end())
            m_taskTrees.erase(it);
    }

    template <typename SetupHandler = TreeSetupHandler, typename DoneHandler = TreeDoneHandler>
    void start(const Key &key, const Group &recipe,
               SetupHandler &&setupHandler = {},
               DoneHandler &&doneHandler = {},
               CallDone callDone = CallDoneFlag::Always)
    {
        startImpl(key, recipe,
                  wrapTreeSetupHandler(std::forward<SetupHandler>(setupHandler)),
                  wrapTreeDoneHandler(std::forward<DoneHandler>(doneHandler)),
                  callDone);
    }

private:
    void startImpl(const Key &key, const Group &recipe,
                   const TreeSetupHandler &setupHandler,
                   const TreeDoneHandler &doneHandler,
                   CallDone callDone)
    {
        QTaskTree *taskTree = new QTaskTree(recipe);
        connect(taskTree, &QTaskTree::done,
                this, [this, key, doneHandler, callDone](DoneWith result) {
            const auto it = m_taskTrees.find(key);
            QTaskTree *runningTaskTree = it->second.release();
            runningTaskTree->deleteLater();
            m_taskTrees.erase(it);
            if (doneHandler && shouldCallDone(callDone, result))
                doneHandler(*runningTaskTree, result);
            Q_EMIT done(result, runningTaskTree);
        });
        m_taskTrees[key].reset(taskTree);
        if (setupHandler)
            setupHandler(*taskTree);
        Q_EMIT aboutToStart(taskTree);
        taskTree->start();
    }
    std::unordered_map<Key, std::unique_ptr<QTaskTree>> m_taskTrees;
};

} // namespace QtTaskTree

QT_END_NAMESPACE

#endif // QTASKTREERUNNER_H
