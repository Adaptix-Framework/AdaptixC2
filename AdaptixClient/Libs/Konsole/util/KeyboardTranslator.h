#ifndef KEYBOARDTRANSLATOR_H
#define KEYBOARDTRANSLATOR_H

#include <QHash>
#include <QList>
#include <QKeySequence>
#include <QMetaType>
#include <QVarLengthArray>

class QIODevice;
class QTextStream;

class KeyboardTranslator
{
public:

    enum State {
        NoState = 0,
        NewLineState = 1,
        AnsiState = 2,
        CursorKeysState = 4,
        AlternateScreenState = 8,
        AnyModifierState = 16,
        ApplicationKeypadState = 32
    };
    Q_DECLARE_FLAGS(States,State)

    enum Command {
        NoCommand = 0,
        SendCommand = 1,
        ScrollPageUpCommand = 2,
        ScrollPageDownCommand = 4,
        ScrollLineUpCommand = 8,
        ScrollLineDownCommand = 16,
        ScrollLockCommand = 32,
        ScrollUpToTopCommand = 64,
        ScrollDownToBottomCommand = 128,
        EraseCommand = 256
    };
    Q_DECLARE_FLAGS(Commands,Command)

    class Entry {
    public:
        Entry();

        bool isNull() const;

        Command command() const;
        void setCommand(Command command);

        QByteArray text(bool expandWildCards = false, Qt::KeyboardModifiers modifiers = Qt::NoModifier) const;

        void setText(const QByteArray& text);

        QByteArray escapedText(bool expandWildCards = false, Qt::KeyboardModifiers modifiers = Qt::NoModifier) const;

        int keyCode() const;
        void setKeyCode(int keyCode);

        Qt::KeyboardModifiers modifiers() const;

        Qt::KeyboardModifiers modifierMask() const;

        void setModifiers( Qt::KeyboardModifiers modifiers );
        void setModifierMask( Qt::KeyboardModifiers modifiers );

        States state() const;

        States stateMask() const;

        void setState( States state );
        void setStateMask( States mask );

        QString conditionToString() const;

        QString resultToString(bool expandWildCards = false, Qt::KeyboardModifiers modifiers = Qt::NoModifier) const;

        bool matches( int keyCode , Qt::KeyboardModifiers modifiers , States flags ) const;

        bool operator==(const Entry& rhs) const;

    private:
        void insertModifier( QString& item , int modifier ) const;
        void insertState( QString& item , int state ) const;
        QByteArray unescape(const QByteArray& text) const;

        int _keyCode;
        Qt::KeyboardModifiers _modifiers;
        Qt::KeyboardModifiers _modifierMask;
        States _state;
        States _stateMask;

        Command _command;
        QByteArray _text;
    };

    KeyboardTranslator(const QString& name);

    QString name() const;

    void setName(const QString& name);

    QString description() const;

    void setDescription(const QString& description);

    Entry findEntry(int keyCode , Qt::KeyboardModifiers modifiers , States state = NoState) const;

    void addEntry(const Entry& entry);

    void replaceEntry(const Entry& existing , const Entry& replacement);

    void removeEntry(const Entry& entry);

    QList<Entry> entries() const;

    static const Qt::KeyboardModifier CTRL_MOD;

private:
    QMultiHash<int,Entry> _entries;
    QString _name;
    QString _description;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(KeyboardTranslator::States)
Q_DECLARE_OPERATORS_FOR_FLAGS(KeyboardTranslator::Commands)

class KeyboardTranslatorReader
{
public:
    KeyboardTranslatorReader( QIODevice* source );

    QString description() const;

    bool hasNextEntry() const;
    KeyboardTranslator::Entry nextEntry();

    bool parseError();

    static KeyboardTranslator::Entry createEntry( const QString& condition ,
                                                  const QString& result );
private:
    struct Token {
        enum Type {
            TitleKeyword,
            TitleText,
            KeyKeyword,
            KeySequence,
            Command,
            OutputText
        };
        Type type;
        QString text;
    };
    QList<Token> tokenize(const QString&);
    void readNext();
    bool decodeSequence(const QString& ,
                                int& keyCode,
                                Qt::KeyboardModifiers& modifiers,
                                Qt::KeyboardModifiers& modifierMask,
                                KeyboardTranslator::States& state,
                                KeyboardTranslator::States& stateFlags);

    static bool parseAsModifier(const QString& item , Qt::KeyboardModifier& modifier);
    static bool parseAsStateFlag(const QString& item , KeyboardTranslator::State& state);
    static bool parseAsKeyCode(const QString& item , int& keyCode);
    static bool parseAsCommand(const QString& text , KeyboardTranslator::Command& command);

    QIODevice* _source;
    QString _description;
    KeyboardTranslator::Entry _nextEntry;
    bool _hasNext;

    KeyboardTranslatorReader(const KeyboardTranslatorReader&) = delete;
    KeyboardTranslatorReader& operator=(const KeyboardTranslatorReader&) = delete;
};

class KeyboardTranslatorWriter
{
public:
    KeyboardTranslatorWriter(QIODevice* destination);
    ~KeyboardTranslatorWriter();

    void writeHeader( const QString& description );
    void writeEntry( const KeyboardTranslator::Entry& entry );

private:
    QIODevice* _destination;
    QTextStream* _writer;

    KeyboardTranslatorWriter(const KeyboardTranslatorWriter&) = delete;
    KeyboardTranslatorWriter& operator=(const KeyboardTranslatorWriter&) = delete;
};

class KeyboardTranslatorManager
{
public:
    KeyboardTranslatorManager();
    ~KeyboardTranslatorManager();

    KeyboardTranslatorManager(const KeyboardTranslatorManager&) = delete;
    KeyboardTranslatorManager& operator=(const KeyboardTranslatorManager&) = delete;

    void addTranslator(KeyboardTranslator* translator);

    bool deleteTranslator(const QString& name);

    const KeyboardTranslator* defaultTranslator();

    const KeyboardTranslator* findTranslator(const QString& name);
    QList<QString> allTranslators();

    static KeyboardTranslatorManager* instance();

private:
    static const QByteArray defaultTranslatorText;
    
    QString get_kb_layout_dir();
    void findTranslators();
    KeyboardTranslator* loadTranslator(const QString& name);
    KeyboardTranslator* loadTranslator(QIODevice* device,const QString& name);

    bool saveTranslator(const KeyboardTranslator* translator);
    QString findTranslatorPath(const QString& name);

    QHash<QString,KeyboardTranslator*> _translators;
    bool _haveLoadedAll;
};

inline int KeyboardTranslator::Entry::keyCode() const { 
    return _keyCode; 
}

inline void KeyboardTranslator::Entry::setKeyCode(int keyCode) { 
    _keyCode = keyCode; 
}

inline void KeyboardTranslator::Entry::setModifiers( Qt::KeyboardModifiers modifier ) {
    _modifiers = modifier;
}

inline Qt::KeyboardModifiers KeyboardTranslator::Entry::modifiers() const {
    return _modifiers; 
}

inline void  KeyboardTranslator::Entry::setModifierMask( Qt::KeyboardModifiers mask ) {
   _modifierMask = mask;
}

inline Qt::KeyboardModifiers KeyboardTranslator::Entry::modifierMask() const {
    return _modifierMask; 
}

inline bool KeyboardTranslator::Entry::isNull() const {
    return ( *this == Entry() );
}

inline void KeyboardTranslator::Entry::setCommand( Command command ) {
    _command = command;
}

inline KeyboardTranslator::Command KeyboardTranslator::Entry::command() const { 
    return _command; 
}

inline void KeyboardTranslator::Entry::setText( const QByteArray& text ) {
    _text = unescape(text);
}

inline int oneOrZero(int value) {
    return value ? 1 : 0;
}

inline QByteArray KeyboardTranslator::Entry::text(bool expandWildCards,Qt::KeyboardModifiers modifiers) const {
    QByteArray expandedText = _text;

    if (expandWildCards) {
        int modifierValue = 1;
        modifierValue += oneOrZero(modifiers & Qt::ShiftModifier);
        modifierValue += oneOrZero(modifiers & Qt::AltModifier) << 1;
        modifierValue += oneOrZero(modifiers & KeyboardTranslator::CTRL_MOD) << 2;

        for (int i=0;i<_text.length();i++) {
            if (expandedText[i] == '*')
                expandedText[i] = '0' + modifierValue;
        }
    }

    return expandedText;
}

inline void KeyboardTranslator::Entry::setState( States state ) {
    _state = state;
}

inline KeyboardTranslator::States KeyboardTranslator::Entry::state() const { 
    return _state; 
}

inline void KeyboardTranslator::Entry::setStateMask( States stateMask ) {
    _stateMask = stateMask;
}

inline KeyboardTranslator::States KeyboardTranslator::Entry::stateMask() const { 
    return _stateMask; 
}

Q_DECLARE_METATYPE(KeyboardTranslator::Entry)
Q_DECLARE_METATYPE(const KeyboardTranslator*)

#endif
