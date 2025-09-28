#ifndef HISTOTYSEARCH_H
#define	HISTOTYSEARCH_H

#include <QObject>
#include <QPointer>
#include <QMap>
#include <QRegularExpression>

#include "../ScreenWindow.h"

#include "../Emulation.h"
#include "TerminalCharacterDecoder.h"

typedef QPointer<Emulation> EmulationPtr;

class HistorySearch : public QObject
{
    Q_OBJECT

public:
    explicit HistorySearch(EmulationPtr emulation, const QRegularExpression& regExp, bool forwards,
                           int startColumn, int startLine, QObject* parent);
    ~HistorySearch() override;
    void search();

Q_SIGNALS:
    void matchFound(int startColumn, int startLine, int endColumn, int endLine);
    void noMatchFound();

private:
    bool search(int startColumn, int startLine, int endColumn, int endLine);
    int findLineNumberInString(QList<int> linePositions, int position);

    EmulationPtr m_emulation;
    QRegularExpression m_regExp;
    bool m_forwards = false;
    int m_startColumn = 0;
    int m_startLine = 0;

    int m_foundStartColumn = 0;
    int m_foundStartLine = 0;
    int m_foundEndColumn = 0;
    int m_foundEndLine = 0;
};

#endif	/* HISTOTYSEARCH_H */
