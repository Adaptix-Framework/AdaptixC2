#ifndef ADAPTIXCLIENT_LOGVIEWTAPE_H
#define ADAPTIXCLIENT_LOGVIEWTAPE_H

#include <string>
#include <vector>

struct LogViewBlock {
    std::string id;
    std::string role; // system|user|assistant|tool|error
    std::string text;
    bool        open     = true;
    bool        expanded = true;
};

class LogViewTape {
    std::vector<LogViewBlock> m_blocks;
    int                       m_nextId = 1;
    bool                      m_autoScroll = true;

    LogViewBlock* find(const std::string& blockId);
    const LogViewBlock* find(const std::string& blockId) const;

public:
    static bool isValidRole(const std::string& role);
    static std::string normalizeRole(const std::string& role);

    std::string append(const std::string& role, const std::string& text);
    bool        appendDelta(const std::string& blockId, const std::string& text);
    bool        endBlock(const std::string& blockId);
    bool        toggleExpanded(const std::string& blockId);
    void        clear();

    void setAutoScroll(bool enabled);
    bool autoScroll() const;

    const std::vector<LogViewBlock>& blocks() const { return m_blocks; }
    int  count() const { return static_cast<int>(m_blocks.size()); }
};

#endif
