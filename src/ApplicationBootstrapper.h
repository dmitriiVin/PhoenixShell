#pragma once

#include <QString>
#include <QObject>
#include <memory>

class ToolRegistry;
class DataLoader;
struct DriveInfo;

/**
 * @brief Загрузчик приложения при старте
 * 
 * Отвечает за инициализацию всех компонентов PhoenixShell:
 * 1. Определение разделов флешки
 * 2. Загрузка инструментов, драйверов, образов
 * 3. Загрузка конфигурации и пользовательских параметров
 * 4. Инициализация системы уведомлений
 */
class ApplicationBootstrapper final : public QObject {
    Q_OBJECT

public:
    explicit ApplicationBootstrapper(QObject* parent = nullptr);
    ~ApplicationBootstrapper();

    /**
     * @brief Выполнить полную инициализацию приложения
     * @param toolRegistry Реестр инструментов для регистрации найденных инструментов
     * @return true если инициализация успешна, false если возникли критические ошибки
     */
    bool initialize(ToolRegistry* toolRegistry);

    /**
     * @brief Получить букву раздела данных (если найден)
     */
    QString getDataPartitionLetter() const { return m_dataPartitionLetter; }

    /**
     * @brief Получить указатель на загрузчик данных (если инициализирован)
     */
    DataLoader* getDataLoader() const { return m_dataLoader.get(); }

    /**
     * @brief Получить информацию о найденном разделе данных
     */
    const DriveInfo* getDataPartitionInfo() const { return m_dataPartitionInfo.get(); }

    /**
     * @brief Проверить, является ли это WinPE средой
     */
    static bool isWinPEEnvironment();

    /**
     * @brief Получить версию приложения с добавленной информацией об окружении
     */
    static QString getVersionInfo();

signals:
    void initializationProgress(const QString& message);
    void initializationComplete();
    void initializationError(const QString& errorMessage);

private:
    void logProgress(const QString& message);
    void logError(const QString& errorMessage);

    QString m_dataPartitionLetter;
    std::unique_ptr<DataLoader> m_dataLoader;
    std::unique_ptr<DriveInfo> m_dataPartitionInfo;
};
