
#include <QApplication>
#include <QDebug>
#include <QRegularExpressionMatch>
#include <QTextStream>

#include "../Emulation.h"
#include "HistorySearch.h"
#include "TerminalCharacterDecoder.h"

HistorySearch::HistorySearch(EmulationPtr emulation, const QRegularExpression &regExp, bool forwards, int startColumn, int startLine, QObject *parent)
    : QObject(parent), m_emulation(emulation), m_regExp(regExp),
      m_forwards(forwards), m_startColumn(startColumn), m_startLine(startLine) {
}

HistorySearch::~HistorySearch() {
}

void HistorySearch::search() {
    bool found = false;

    if (!m_regExp.pattern().isEmpty()) {
        if (m_forwards) {
        found =
            search(m_startColumn, m_startLine, -1, m_emulation->lineCount()) ||
            search(0, 0, m_startColumn, m_startLine);
        } else {
            found = search(0, 0, m_startColumn, m_startLine) ||
                search(m_startColumn, m_startLine, -1, m_emulation->lineCount());
        }

        if (found) {
            emit matchFound(m_foundStartColumn, m_foundStartLine, m_foundEndColumn,
                        m_foundEndLine);
        } else {
            emit noMatchFound();
        }
    }

    deleteLater();
}

bool HistorySearch::search(int startColumn, int startLine, int endColumn,
                           int endLine) {
    int linesRead = 0;
    int linesToRead = endLine - startLine + 1;

    int blockSize;
    while ((blockSize = qMin(10000, linesToRead - linesRead)) > 0) {
        QString string;
        QTextStream searchStream(&string);
        PlainTextDecoder decoder;
        decoder.begin(&searchStream);
        decoder.setRecordLinePositions(true);

        int blockStartLine = m_forwards ? startLine + linesRead : endLine - linesRead - blockSize + 1;
        int chunkEndLine = blockStartLine + blockSize - 1;
        m_emulation->writeToStream(&decoder, blockStartLine, chunkEndLine);

        int endPosition;

        int numberOfLinesInString = decoder.linePositions().size() - 1;
        if (numberOfLinesInString > 0 && endColumn > -1) {
            endPosition = decoder.linePositions().at(numberOfLinesInString - 1) + endColumn;
        } else {
            endPosition = string.size();
        }

        int matchStart;
        QRegularExpressionMatch match;
        if (m_forwards) {
            matchStart = string.indexOf(m_regExp, startColumn, &match);
        if (matchStart >= endPosition)
            matchStart = -1;
        } else {
            matchStart = string.lastIndexOf(m_regExp, endPosition - 1, &match);
            if (matchStart < startColumn)
                matchStart = -1;
        }

        if (matchStart > -1) {
            int matchEnd = matchStart + match.capturedLength() - 1;

            int startLineNumberInString = findLineNumberInString(decoder.linePositions(), matchStart);
            m_foundStartColumn = matchStart - decoder.linePositions().at(startLineNumberInString);
            m_foundStartLine = startLineNumberInString + startLine + linesRead;

            int endLineNumberInString = findLineNumberInString(decoder.linePositions(), matchEnd);
            m_foundEndColumn = matchEnd - decoder.linePositions().at(endLineNumberInString);
            m_foundEndLine = endLineNumberInString + startLine + linesRead;

            return true;
        }

        linesRead += blockSize;
    }

    return false;
}

int HistorySearch::findLineNumberInString(QList<int> linePositions,
                                          int position) {
    int lineNum = 0;
    while (lineNum + 1 < linePositions.size() &&
            linePositions[lineNum + 1] <= position)
        lineNum++;

    return lineNum;
}
