#include <Utils/CustomElements/LogViewTape.h>

#include <cctype>
#include <string>

std::string LogViewTape::normalizeRole(const std::string& role)
{
    std::string out;
    out.reserve(role.size());
    for (unsigned char c : role)
        out.push_back(static_cast<char>(std::tolower(c)));
    return out;
}

bool LogViewTape::isValidRole(const std::string& role)
{
    const std::string r = normalizeRole(role);
    return r == "system" || r == "user" || r == "assistant" || r == "tool" || r == "error";
}

LogViewBlock* LogViewTape::find(const std::string& blockId)
{
    for (auto& b : m_blocks) {
        if (b.id == blockId)
            return &b;
    }
    return nullptr;
}

const LogViewBlock* LogViewTape::find(const std::string& blockId) const
{
    for (const auto& b : m_blocks) {
        if (b.id == blockId)
            return &b;
    }
    return nullptr;
}

std::string LogViewTape::append(const std::string& role, const std::string& text)
{
    std::string r = normalizeRole(role);
    if (!isValidRole(r))
        r = "system";

    LogViewBlock block;
    block.id   = "b" + std::to_string(m_nextId++);
    block.role = r;
    block.text = text;
    block.open = true;
    block.expanded = (r != "tool");
    m_blocks.push_back(std::move(block));
    return m_blocks.back().id;
}

bool LogViewTape::appendDelta(const std::string& blockId, const std::string& text)
{
    LogViewBlock* block = find(blockId);
    if (!block || !block->open)
        return false;
    block->text += text;
    return true;
}

bool LogViewTape::endBlock(const std::string& blockId)
{
    LogViewBlock* block = find(blockId);
    if (!block)
        return false;
    block->open = false;
    return true;
}

bool LogViewTape::toggleExpanded(const std::string& blockId)
{
    LogViewBlock* block = find(blockId);
    if (!block)
        return false;
    block->expanded = !block->expanded;
    return true;
}

void LogViewTape::clear()
{
    m_blocks.clear();
}

void LogViewTape::setAutoScroll(bool enabled)
{
    m_autoScroll = enabled;
}

bool LogViewTape::autoScroll() const
{
    return m_autoScroll;
}
