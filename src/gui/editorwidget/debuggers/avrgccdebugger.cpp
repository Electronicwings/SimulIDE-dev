/***************************************************************************
 *   Copyright (C) 2021 by Santiago González                               *
 *                                                                         *
 ***( see copyright.txt file at root folder )*******************************/

#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QSet>

#include "avrgccdebugger.h"
#include "e_mcu.h"
#include "outpaneltext.h"
#include "codeeditor.h"
#include "utils.h"

#ifdef SIM_HAS_LIBBFD
// bfd.h refuses to compile unless either binutils' generated config.h was
// already included or PACKAGE/PACKAGE_VERSION are defined. Including
// binutils' config.h drags in many HAVE_* macros that may conflict with
// Qt/other code in this translation unit, so we define only the two
// macros bfd.h actually checks for.
#ifndef PACKAGE
#define PACKAGE "simulide-libbfd"
#endif
#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION "1.0"
#endif
extern "C" {
    #include <bfd.h>
    #include <dis-asm.h>
    #include <zlib.h>
}
#include <cstdlib>
#include <cstdarg>

// emcc 1.39.x's bundled zlib port lacks `uncompress2` (added in zlib
// 1.2.9). libbfd's compressed-debug-section path references it
// unconditionally. Provide the missing symbol via a small wrapper around
// the always-present `uncompress`. Defined as weak so a real uncompress2
// from a newer zlib supersedes it without conflict.
extern "C" __attribute__((weak))
int uncompress2( Bytef* dest, uLongf* destLen,
                 const Bytef* source, uLong* sourceLen )
{
    int rc = uncompress( dest, destLen, source, *sourceLen );
    // The contract for uncompress2 reports how many source bytes were
    // actually consumed; the simpler uncompress doesn't. Best-effort: on
    // success report the same value the caller passed in.
    return rc;
}
#endif

AvrGccDebugger::AvrGccDebugger( CodeEditor* parent, OutPanelText* outPane )
              : cDebugger( parent, outPane )
{
#ifndef __EMSCRIPTEN__
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert( QStringLiteral("LANG"), QStringLiteral("C") );
    env.insert( QStringLiteral("LC_MESSAGES"), QStringLiteral("C") );
    m_compProcess.setProcessEnvironment( env );
#endif
    m_addrBytes = 1; // Default for avr-gcc
}
AvrGccDebugger::~AvrGccDebugger(){}

bool AvrGccDebugger::postProcess()
{
    m_elfPath = m_buildPath+m_fileName+".elf";
    if( !QFileInfo::exists( m_elfPath ) )
    {
        m_outPane->appendLine( "\n"+QObject::tr("Warning: elf file doesn't exist:")+"\n"+m_elfPath );
        return false;
    }

#ifdef SIM_HAS_LIBBFD
    // Wasm build: avr-* CLI tools can't be spawned (no QProcess), so call
    // libbfd in-process. Don't quote the path — libbfd uses fopen() which
    // would treat the surrounding quotes as part of the filename.
    bool ok = getVariablesBfd();
    if( !ok ) return false;
    ok = getFunctionsBfd();
    if( !ok ) return false;
    m_flashToSource.clear();
    if( !mapFlashToSourceBfd() ) return false;
    disassembleAllBfd(); // not fatal if it fails — UI just shows empty
    return true;
#else
    m_elfPath = addQuotes( m_elfPath );
    bool ok = getVariables();
    if( !ok ) return false;
    ok = getFunctions();
    if( !ok ) return false;
    m_flashToSource.clear();
    return mapFlashToSource();
#endif
}

bool AvrGccDebugger::getVariables()
{
#ifdef __EMSCRIPTEN__
    return false;
#else
    QString objdump = m_toolPath+"avr/bin/avr-objdump";

#ifndef Q_OS_UNIX
    objdump += ".exe";
#endif

    if( !checkCommand( objdump ) )
    {
        objdump = m_toolPath+"avr-objdump";
    #ifndef Q_OS_UNIX
        objdump += ".exe";
    #endif
        if( !checkCommand( objdump ) )
            m_outPane->appendLine( "\nWarning: avr-objdump executable not detected:\n"+objdump );
    }
    m_outPane->appendText( "\nSearching for variables... " );
    objdump = addQuotes( objdump );

    QProcess getBss( nullptr );      // Get var addresses from .bss section
    QString command  = objdump+" -t -j.bss "+m_elfPath;
    getBss.start( command );
    getBss.waitForFinished(-1);

    QString  p_stdout = getBss.readAllStandardOutput();
    QStringList varNames = m_varTypes.keys();
    QStringList varList;
    //m_subs.clear();

    for( QString line : p_stdout.split("\n") )
    {
        if( line.isEmpty() ) continue;
        QStringList words = line.split(" ");
        if( words.size() < 9 ) continue;
        if( words.at(6) != "O" ) continue;

        QString addr   = words.at(0);
        bool ok = false;
        int address = addr.toInt( &ok, 16 );
        if( !ok ) continue;

        QString symbol = words.at(8);
        QString type;

        if( varNames.contains( symbol ) ) type = m_varTypes.value( symbol );
        else{
            QString size = words.at(7);
            size = size.split("\t").last();
            type = "u"+QString::number( size.toInt()*8 );
        }
        address -= 0x800000;          // 0x800000 offset

        eMcu::self()->getRamTable()->addVariable( symbol, address, type );
        varList.append( symbol );
        //qDebug() << "AvrGccDebugger::getAvrGccData  variable "<<type<<symbol<<address;
    }
    eMcu::self()->getRamTable()->setVariables( varList );
    m_outPane->appendLine( QString::number( varList.size() )+" variables found" );
    return true;
#endif
}

bool AvrGccDebugger::getFunctions()
{
#ifdef __EMSCRIPTEN__
    return false;
#else
    // avr-readelf -s file.elf
    //   Num:    Valor  Tam  Tipo    Unión  Vis      Nombre Ind
    //    34: 00000090    22 FUNC    GLOBAL DEFAULT    2 function_name
    //    27: 00800100     1 OBJECT  GLOBAL DEFAULT    3 variable_name

    QString readelf = m_toolPath+"avr/bin/avr-readelf";

#ifndef Q_OS_UNIX
    readelf += ".exe";
#endif

    if( !checkCommand( readelf ) )
    {
        readelf = m_toolPath+"avr-readelf";
    #ifndef Q_OS_UNIX
        readelf += ".exe";
    #endif
        if( !checkCommand( readelf ) )
            m_outPane->appendLine( "\nWarning: avr-readelf executable not detected:\n"+readelf );
    }
    m_outPane->appendText( "\nSearching for Functions... " );
    readelf = addQuotes( readelf );

    QProcess getFunctions( nullptr );      //
    QString command  = readelf+" -s "+m_elfPath;
    getFunctions.start( command );
    getFunctions.waitForFinished(-1);

    QString  p_stdout = getFunctions.readAllStandardOutput();

    for( QString line : p_stdout.split("\n") )
    {
        if( line.isEmpty() ) continue;
        //qDebug() << "AvrGccDebugger::getFunctions "<< line;
        QStringList words = line.split(" ");
        words.removeAll("");
        if( words.size() < 8 ) continue;
        if( words.at(3) != "FUNC" ) continue;

        QString funcName = words.last();
        //if( funcName.startsWith("_") ) continue;

        QString bind = words.at(4);
        //if( bind != "LOCAL") continue;

        QString addr = words.at(1);
        bool ok = false;
        int address = addr.toInt( &ok, 16 )/2;
        if( !ok ) continue;

        m_functions[funcName] = address;
        //qDebug() << "AvrGccDebugger::getFunctions "<< funcName <<address;
    }
    m_outPane->appendLine( QString::number( m_functions.size() )+" functions found" );
    return true;
#endif
}

bool AvrGccDebugger::mapFlashToSource()
{
#ifdef __EMSCRIPTEN__
    return false;
#else
    QString avrSize = m_toolPath+"avr/bin/avr-size";
    QString addr2li = m_toolPath+"avr/bin/avr-addr2line";

#ifndef Q_OS_UNIX
    avrSize += ".exe";
    addr2li += ".exe";
#endif
    if( !checkCommand( avrSize ) )
    {
        avrSize = m_toolPath+"avr-size";
    #ifndef Q_OS_UNIX
        avrSize += ".exe";
    #endif
        if( !checkCommand( avrSize ) )
            m_outPane->appendLine( "\nWarning: avr-size executable not detected:\n"+avrSize );
    }
    if( !checkCommand( addr2li ) )
    {
        addr2li = m_toolPath+"avr-addr2line";
    #ifndef Q_OS_UNIX
        addr2li += ".exe";
    #endif
        if( !checkCommand( addr2li ) )
            m_outPane->appendLine( "\nWarning: avr-addr2line executable not detected:\n"+addr2li );
    }

    m_outPane->appendText( "\nMapping Flash to Source... " );
    avrSize = addQuotes( avrSize );
    addr2li = addQuotes( addr2li );

    QProcess getSize( 0 );  // Get Firmware size
    getSize.start( avrSize + " " + m_elfPath );
    getSize.waitForFinished();
    QString lines = getSize.readAllStandardOutput();

    getSize.close();
    bool ok = false;
    int flashSize;
    if( !lines.isEmpty() )
    {
        QString size = lines.split("\n").at(1);
        size = size.split("\t").takeFirst().remove(" ");
        flashSize = size.toInt( &ok );
    }
    if( !ok || flashSize == 0 ) flashSize = 35000;

    QProcess flashToLine( 0 );
    flashToLine.start( addr2li + " -e " + m_elfPath );
    ok = flashToLine.waitForStarted( 1000 );
    if( ok )
    {
        for( int flashAddr=0; flashAddr<flashSize; ++flashAddr )
        {
            QString addr = val2hex( flashAddr )+"\n";
            flashToLine.write( addr.toUtf8() );
        }
        flashToLine.closeWriteChannel();
        flashToLine.waitForFinished();

        //m_lastLine = 0;
        for( int flashAddr=0; flashAddr<flashSize; ++flashAddr ) // Map Flash Address to Source Line
        {
            QString p_stdout = flashToLine.readLine();
            if( p_stdout.isEmpty() ) continue;
            if( p_stdout.startsWith("?") ) continue;

            int idx = p_stdout.lastIndexOf( ":" );
            if( idx == -1 ) continue;

            QString filePath = QFileInfo( p_stdout.left( idx ) ).absoluteFilePath();// filePath();//  .fileName();
            if( !QFile::exists( filePath ) ) continue;

            if( !m_fileList.contains( filePath ) ) continue;
            /*{
                //qDebug() << "AvrGccDebugger::mapFlashToSource" << filePath;
                m_fileList.append( filePath );
            }*/

            bool ok = false;
            int line = p_stdout.mid( idx+1 ).toInt( &ok );
            if( !ok ) continue;
            int addr = flashAddr*m_addrBytes/2;
            //qDebug() << QString::number(flashAddr,16)<<addr<<line <<" -----> "<< p_stdout;

            setLineToFlash( {filePath,line}, addr );
        }
        flashToLine.close();
    }
    ok = !m_flashToSource.isEmpty();
    m_outPane->appendLine( QString::number( m_flashToSource.size() )+" lines mapped" );
    return ok;
#endif
}

#ifdef SIM_HAS_LIBBFD
// ---------------------------------------------------------------------------
// libbfd-based replacements for the avr-* CLI tools (wasm build).
// ---------------------------------------------------------------------------
//
// These three methods reproduce what avr-objdump/-readelf/-size/-addr2line
// did via QProcess in the native build, using direct libbfd C calls. The
// outcomes feed the same eMcu/RamTable APIs as the QProcess path so the
// debugger UI is identical to a user.

namespace {

bfd* openElf( const QString& path )
{
    bfd_init();
    bfd* abfd = bfd_openr( path.toUtf8().constData(), nullptr );
    if( !abfd ) return nullptr;
    if( !bfd_check_format( abfd, bfd_object ) ){
        bfd_close( abfd );
        return nullptr;
    }
    return abfd;
}

// Read the symbol table into an asymbol** array. Caller frees with free().
// Returns the count, 0 if there is no symtab or on allocation failure.
long readSymtab( bfd* abfd, asymbol*** out )
{
    *out = nullptr;
    long storage = bfd_get_symtab_upper_bound( abfd );
    if( storage <= 0 ) return 0;
    *out = static_cast<asymbol**>( std::malloc( static_cast<size_t>(storage) ) );
    if( !*out ) return 0;
    long n = bfd_canonicalize_symtab( abfd, *out );
    if( n < 0 ){ std::free( *out ); *out = nullptr; return 0; }
    return n;
}

} // anonymous namespace

bool AvrGccDebugger::getVariablesBfd()
{
    m_outPane->appendText( "\nSearching for variables... " );

    bfd* abfd = openElf( m_elfPath );
    if( !abfd ){
        m_outPane->appendLine( "elf open failed" );
        return false;
    }
    asymbol** syms = nullptr;
    long n = readSymtab( abfd, &syms );
    asection* bss = bfd_get_section_by_name( abfd, ".bss" );

    QStringList varNames = m_varTypes.keys();
    QStringList varList;

    for( long i = 0; i < n; ++i ){
        asymbol* s = syms[i];
        if( s->section != bss ) continue;
        if( !(s->flags & BSF_OBJECT) ) continue;

        int address = static_cast<int>( bfd_asymbol_value( s ) ) - 0x800000; // strip avr-libc data offset
        QString symbol = QString::fromUtf8( bfd_asymbol_name( s ) );

        QString type;
        if( varNames.contains( symbol ) ){
            type = m_varTypes.value( symbol );
        } else {
            // Use the symbol's own size (in bytes) as a fallback type hint.
            // bfd_asymbol_size is preferred but not always populated; fall
            // back to "u8" if unavailable.
            int sz = static_cast<int>( s->udata.i ); // not always meaningful
            if( sz <= 0 ) sz = 1;
            type = "u" + QString::number( sz * 8 );
        }
        eMcu::self()->getRamTable()->addVariable( symbol, address, type );
        varList.append( symbol );
    }
    eMcu::self()->getRamTable()->setVariables( varList );

    if( syms ) std::free( syms );
    bfd_close( abfd );

    m_outPane->appendLine( QString::number( varList.size() )+" variables found" );
    return true;
}

bool AvrGccDebugger::getFunctionsBfd()
{
    m_outPane->appendText( "\nSearching for Functions... " );

    bfd* abfd = openElf( m_elfPath );
    if( !abfd ){
        m_outPane->appendLine( "elf open failed" );
        return false;
    }
    asymbol** syms = nullptr;
    long n = readSymtab( abfd, &syms );

    for( long i = 0; i < n; ++i ){
        asymbol* s = syms[i];
        if( !(s->flags & BSF_FUNCTION) ) continue;

        // /2 mirrors the original parser — AVR addresses come back in bytes,
        // but eMcu indexes flash by 16-bit word, hence the divide.
        int address = static_cast<int>( bfd_asymbol_value( s ) ) / 2;
        m_functions[ QString::fromUtf8( bfd_asymbol_name( s ) ) ] = address;
    }

    if( syms ) std::free( syms );
    bfd_close( abfd );

    m_outPane->appendLine( QString::number( m_functions.size() )+" functions found" );
    return true;
}

bool AvrGccDebugger::mapFlashToSourceBfd()
{
    m_outPane->appendText( "\nMapping Flash to Source... " );

    bfd* abfd = openElf( m_elfPath );
    if( !abfd ){
        m_outPane->appendLine( "elf open failed" );
        return false;
    }
    asection* text = bfd_get_section_by_name( abfd, ".text" );
    int flashSize = text ? static_cast<int>( bfd_section_size( text ) ) : 35000;

    asymbol** syms = nullptr;
    long n = readSymtab( abfd, &syms );

    // In the wasm/remote-build flow:
    //   - cDebugger::preProcess populates m_fileList from .c/.cpp/.h/.hpp
    //     only; .ino sketches aren't in there.
    //   - DWARF paths reference the REMOTE build host's filesystem (e.g.
    //     /tmp/arduino_<uuid>/sketch.ino), so absolute-path comparisons
    //     against local files always fail.
    //
    // Workaround: match by **basename** against the editor's current file
    // (m_file, inherited from cDebugger) first, then fall back to
    // basename-matching against m_fileList for any extra local sources.
    const QString localFile = m_file;
    const QString localBase = QFileInfo( localFile ).fileName();

    int matchedHits = 0;

    for( int flashAddr = 0; flashAddr < flashSize; ++flashAddr ){
        const char* file = nullptr;
        const char* func = nullptr;
        unsigned int line = 0;

        if( !text ) continue;
        if( !bfd_find_nearest_line( abfd, text, syms,
                                    static_cast<bfd_vma>(flashAddr),
                                    &file, &func, &line ) )
            continue;
        if( !file || line == 0 ) continue;

        const QString rawBase = QFileInfo( QString::fromUtf8( file ) ).fileName();

        QString matchedKey;
        if( !localBase.isEmpty() && rawBase == localBase ){
            matchedKey = localFile;        // most common: hit in the open sketch
        } else {
            for( const QString& candidate : m_fileList ){
                if( QFileInfo( candidate ).fileName() == rawBase ){
                    matchedKey = candidate;
                    break;
                }
            }
        }
        if( matchedKey.isEmpty() ) continue; // Arduino core file etc — skip

        int addr = flashAddr * m_addrBytes / 2;
        setLineToFlash( { matchedKey, static_cast<int>(line) }, addr );
        ++matchedHits;

        (void) func; // unused here — name fed to debugger UI elsewhere
    }
    qDebug() << "[AvrGccDebuggerBfd] mapped" << matchedHits
             << "addresses for sketch basename" << localBase;

    if( syms ) std::free( syms );
    bfd_close( abfd );

    m_outPane->appendLine( QString::number( m_flashToSource.size() )+" lines mapped" );
    return !m_flashToSource.isEmpty();
}
// ---------------------------------------------------------------------------
// Disassembly via libopcodes — populates eMcu::setDisAsm with one row per
// instruction. Called from postProcess after mapFlashToSourceBfd, so the
// Disassembly tab in the MCU Monitor has live rows alongside the source
// mapping. Falls back gracefully (returns false) if the elf can't be
// opened or has no .text section.
// ---------------------------------------------------------------------------

namespace {

// libopcodes' fprintf-style callback hands us each formatted token from
// the disassembler. We accumulate them into a per-instruction QString.
struct DisCollector { QString line; };

int disCollect_fprintf( void* data, const char* fmt, ... )
{
    DisCollector* st = static_cast<DisCollector*>(data);
    char buf[256];
    va_list args;
    va_start( args, fmt );
    int n = vsnprintf( buf, sizeof(buf), fmt, args );
    va_end( args );
    if( n > 0 ) st->line.append( QString::fromUtf8( buf, qMin<int>(n, sizeof(buf)-1) ) );
    return n;
}

// Newer libopcodes also calls fprintf_styled_func. We ignore the style
// enum and forward to the same accumulator.
int disCollect_styled( void* data, enum disassembler_style /*sty*/, const char* fmt, ... )
{
    DisCollector* st = static_cast<DisCollector*>(data);
    char buf[256];
    va_list args;
    va_start( args, fmt );
    int n = vsnprintf( buf, sizeof(buf), fmt, args );
    va_end( args );
    if( n > 0 ) st->line.append( QString::fromUtf8( buf, qMin<int>(n, sizeof(buf)-1) ) );
    return n;
}

} // namespace

bool AvrGccDebugger::disassembleAllBfd()
{
    bfd* abfd = openElf( m_elfPath );
    if( !abfd ) return false;

    asection* text = bfd_get_section_by_name( abfd, ".text" );
    if( !text ){ bfd_close( abfd ); return false; }

    const bfd_size_type size = bfd_section_size( text );
    QVector<bfd_byte> buf( static_cast<int>(size) );
    if( !bfd_get_section_contents( abfd, text, buf.data(), 0, size ) ){
        bfd_close( abfd );
        return false;
    }

    asymbol** syms = nullptr;
    long nSyms = readSymtab( abfd, &syms );

    DisCollector collector;
    disassemble_info info;
    init_disassemble_info( &info, &collector,
                           (fprintf_ftype) disCollect_fprintf,
                           (fprintf_styled_ftype) disCollect_styled );
    info.arch          = bfd_arch_avr;
    info.mach          = bfd_mach_avr5;          // generic ATmega-class default
    info.buffer_vma    = bfd_section_vma( text );
    info.buffer        = buf.data();
    info.buffer_length = size;
    disassemble_init_for_target( &info );

    disassembler_ftype dis = disassembler( bfd_arch_avr, false /*little-endian*/,
                                           bfd_mach_avr5, abfd );
    if( !dis ){
        if( syms ) std::free( syms );
        bfd_close( abfd );
        return false;
    }

    const QString localFile = m_file;
    const QString localBase = QFileInfo( localFile ).fileName();

    QVector<eMcu::DisAsmRow> rows;
    rows.reserve( static_cast<int>(size / 2) );

    bfd_vma pc     = info.buffer_vma;
    bfd_vma endPc  = pc + size;

    while( pc < endPc )
    {
        collector.line.clear();
        int nbytes = dis( pc, &info );
        if( nbytes <= 0 ) break;          // malformed instruction — bail

        eMcu::DisAsmRow row;
        row.pc   = static_cast<uint32_t>( pc );
        row.text = collector.line;
        row.line = 0;

        // Cross-reference to source via the same basename-matching rules
        // mapFlashToSourceBfd uses, so the Disassembly tab agrees with
        // the source view.
        const char*  file    = nullptr;
        const char*  func    = nullptr;
        unsigned int srcLine = 0;
        if( bfd_find_nearest_line( abfd, text, syms, pc, &file, &func, &srcLine )
         && file && srcLine > 0 )
        {
            const QString rawBase = QFileInfo( QString::fromUtf8( file ) ).fileName();
            if( !localBase.isEmpty() && rawBase == localBase ){
                row.file = localFile;
                row.line = static_cast<int>( srcLine );
            } else {
                for( const QString& candidate : m_fileList ){
                    if( QFileInfo( candidate ).fileName() == rawBase ){
                        row.file = candidate;
                        row.line = static_cast<int>( srcLine );
                        break;
                    }
                }
            }
        }
        (void) nSyms; (void) func;
        rows.append( row );
        pc += nbytes;
    }

    if( syms ) std::free( syms );
    bfd_close( abfd );

    eMcu::self()->setDisAsm( rows );
    qDebug() << "[AvrGccDebuggerBfd] disassembled" << rows.size() << "instructions";
    return true;
}
#endif // SIM_HAS_LIBBFD

