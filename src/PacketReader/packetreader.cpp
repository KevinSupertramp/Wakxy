#include "packetreader.h"
#include "utils.h"
#include <QMessageBox>

PacketReader::PacketReader(QString type, QByteArray packet, QObject* parent) : QObject(parent)
{
    m_packet    = packet;
    m_packetStream = new QDataStream(m_packet);
    m_packetStream->setFloatingPointPrecision(QDataStream::SinglePrecision);

    m_analyzedPacket = QString();
    m_analyzedPacketStream = new QTextStream(&m_analyzedPacket);

    m_type      = type;
    m_size      = 0;
    m_opcode    = 0;

    m_scriptFilename = QString();
    m_script = QString();
    m_scriptEngine = NULL;

    m_header = QByteArray();
    deflated = false;
}

void PacketReader::ReadHeader()
{
    *m_packetStream >> m_size;

    if (GetType() == "CMSG")
    {
        qint8 unk1;
        *m_packetStream >> unk1;
    }

    *m_packetStream >> m_opcode;

    if (!deflated)
    {
        m_header = QByteArray();
        QDataStream stream(m_header);

        stream << m_size;
        stream << m_opcode;
    }
}

bool PacketReader::ScriptFileExist()
{
    QDir dir(QCoreApplication::applicationDirPath());

    #ifdef Q_OS_MAC
        dir.cdUp();
        dir.cdUp();
        dir.cdUp();
    #endif

    m_scriptFilename += dir.absolutePath();
    m_scriptFilename += "/Packets/";
    m_scriptFilename += (GetType() == "CMSG") ? "Client/" : "Server/";
    m_scriptFilename += QString::number(GetOpcode()) + ".js";

    return QFile::exists(m_scriptFilename);
}

bool PacketReader::CompileScript(QString script)
{
    m_analyzedPacket = QString();
    m_analyzedPacketStream->seek(0);
    m_packetStream->device()->seek(0);

    if (!deflated)
        ReadHeader();

    if (m_scriptFilename.isEmpty())
        if (!ScriptFileExist())
            return false;

    if (!m_scriptEngine)
    {
        m_scriptEngine = new QScriptEngine(this);
        m_scriptEngine->globalObject().setProperty("packet", m_scriptEngine->newQObject(this));
    }

    *m_analyzedPacketStream << "Opcode : " << GetOpcode() << "\r\n";
    *m_analyzedPacketStream << "Type : " << GetType() << "\r\n";
    *m_analyzedPacketStream << "Size : " << GetSize() << "\r\n\r\n";
    *m_analyzedPacketStream << "Structure : \r\n\r\n";

    if (script.isEmpty())
    {
        QFile file(m_scriptFilename);

        if (!file.open(QIODevice::ReadOnly))
        {
            file.errorString();
            return false;
        }

        QTextStream in(&file);
        script = in.readAll();
        file.close();

        m_script = script;
    }

    QScriptValue result = m_scriptEngine->evaluate(script);
    if (result.isError())
    {
        QMessageBox::critical(0, tr("Script error"), QString::fromLatin1("%0:%1: %2").arg(m_scriptFilename).arg(result.property("lineNumber").toInt32()).arg(result.toString()));
        return false;
    }

    *m_analyzedPacketStream << "\r\n\r\n Data left : " << Length();

    QByteArray remainingData = m_packetStream->device()->readAll();

    *m_analyzedPacketStream << "\r\n ASCII LEFT: " << Utils::ToASCII(remainingData);
    *m_analyzedPacketStream << "\r\n HEX LEFT: " << Utils::ToHexString(remainingData);

    return true;
}

void PacketReader::Deflate()
{
    if (deflated)
        return;

    QByteArray data = m_packetStream->device()->readAll();
    data = qUncompress(data);


    m_packet = QByteArray();
    m_packet.append(m_header);
    m_packet.append(data);
    delete m_packetStream;

    m_packetStream = new QDataStream(m_packet);
    m_packetStream->setFloatingPointPrecision(QDataStream::SinglePrecision);

    deflated = true;
}

void PacketReader::Log(QVariant name)
{
    *m_analyzedPacketStream << "[LOG] " << name.toString() << "\n\n";
}

int PacketReader::Length()
{
    return (int)m_packetStream->device()->bytesAvailable();
}

void PacketReader::Comment(QString comment)
{
    *m_analyzedPacketStream << "// " << comment << "\n";
}

template <typename T> T PacketReader::Read(QString name)
{
    T v;
    *m_packetStream >> v;

    if (!name.isEmpty())
        *m_analyzedPacketStream << name << " : " << v << "\n\n";

    return v;
}

bool PacketReader::ReadBool(QString name) { return Read<bool>((!name.isEmpty()) ? name + " [Bool]" : QString()); }
qint32 PacketReader::ReadInt(QString name) { return Read<qint32>((!name.isEmpty()) ? name + " [Int]" : QString()); }
quint32 PacketReader::ReadUInt(QString name) { return Read<quint32>((!name.isEmpty()) ? name + " [UInt]" : QString()); }
float PacketReader::ReadFloat(QString name) { return Read<float>((!name.isEmpty()) ? name + " [Float]" : QString()); }
double PacketReader::ReadDouble(QString name) { return Read<double>((!name.isEmpty()) ? name + " [Double]" : QString()); }
qint16 PacketReader::ReadShort(QString name) { return Read<qint16>((!name.isEmpty()) ? name + " [Short]" : QString()); }
quint16 PacketReader::ReadUShort(QString name) { return Read<quint16>((!name.isEmpty()) ? name + " [UShort]" : QString()); }
char PacketReader::ReadByte(QString name){ return Read<qint8>((!name.isEmpty()) ? name + " [Byte]" : QString()); }
uchar PacketReader::ReadUByte(QString name) { return Read<quint8>((!name.isEmpty()) ? name + " [UByte]" : QString()); }
qint64 PacketReader::ReadLong(QString name) { return Read<qint64>((!name.isEmpty()) ? name + " [Long]" : QString()); }
quint64 PacketReader::ReadULong(QString name) { return Read<quint64>((!name.isEmpty()) ? name + " [ULong]" : QString()); }

QString PacketReader::ReadString(quint16 length, QString name)
{
    QByteArray bytes;
    bytes.resize(length);

    for (quint16 i = 0; i < length; ++i)
        bytes[i] = Read<qint8>();

    QString string = QString(bytes);

    if (!name.isEmpty())
        *m_analyzedPacketStream << name << " [String] : " << string << "\n\n";

    return string;
}

QString PacketReader::ReadString(QString name)
{
    uchar size;
    *m_packetStream >> size;

    return ReadString(size, name);
}

QString PacketReader::ReadBigString(QString name)
{
    quint16 size;
    *m_packetStream >> size;

    return ReadString(size, name);
}

void PacketReader::Skip(quint16 size)
{
    for (quint16 i = 0; i < size; ++i)
        Read<qint8>();
}

void PacketReader::DumpBlob(QString filename, qint32 size)
{
    QDir dir(QCoreApplication::applicationDirPath());

    #ifdef Q_OS_MAC
        dir.cdUp();
        dir.cdUp();
        dir.cdUp();
    #endif

    QString path = dir.absolutePath() + "/Dumps/" + QString::number(GetOpcode()) + "_" + filename + ".bin";

    QFile blob(path);
    if (!blob.open(QIODevice::WriteOnly))
        return;

    if (!size)
        return;

    if (size == -1)
        size = Length();

    if (size > 2000)
        return;

    QByteArray data;
    data.resize(size);

    for (quint16 i = 0; i < size; ++i)
        data[i] = Read<qint8>();

    blob.write(data);
    blob.close();
}

void PacketReader::WriteSQL(QString filename, QString fields, QString values)
{
    QDir dir(QCoreApplication::applicationDirPath());

    #ifdef Q_OS_MAC
        dir.cdUp();
        dir.cdUp();
        dir.cdUp();
    #endif

    QString path = dir.absolutePath() + "/SQL/" + QString::number(GetOpcode()) + "_" + filename + ".sql";

    QFile sql(path);
    if (!sql.open(QIODevice::WriteOnly | QIODevice::Append))
        return;

    QString req = "REPLACE INTO `creatures` (" + fields + ") VALUES";
    req += "(" + values + ");";

    QTextStream stream( &sql );
    stream << req << "\n";

    qDebug() << req;

    sql.close();
}
