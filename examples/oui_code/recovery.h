#pragma once

#include "editors.h"

#include <oui/application.h>
#include <oui/settings.h>
#include <oui/tab_view.h>

#include <cstddef>
#include <string>
#include <tuple>
#include <vector>

// The copy of the unsaved documents OUI Code keeps beside its settings, so that a crash, a kill, a power cut or a
// terminal closed under it loses little: the next start reopens them, still unsaved.

enum class Reopened
{
    nothing,
    quietly,
    // After an emergency: the user should hear why the documents are back.
    with_notice
};

// Reopens what was kept, still unsaved, into `tabs`, then forgets it: a document already open from the last
// session takes the kept text.
Reopened reopen_unsaved_documents(olagarro::oui::TabView& tabs, std::vector<Document>& documents,
                                  olagarro::oui::Settings& settings);

// Keeps a copy of the unsaved documents every few seconds, as the settings say. The copy is written only when an
// unsaved document changed since the last one.
class UnsavedBackup
{
  public:
    UnsavedBackup(olagarro::oui::Application& application, olagarro::oui::Settings& settings, const Editors& editors);

    static constexpr int default_interval_seconds{10};

    // Starts, restarts or stops the copies as the settings now say.
    void configure();
    // Writes the copy now, whether or not anything changed. It is also written on quitting with "keep unsaved
    // changes" on, and when the terminal goes away or the system stops OUI Code, when nobody can be asked;
    // `announce` says whether the next start tells the user why the documents are back.
    void keep(bool announce);
    // Drops the copy, for a quit that asked about every unsaved document.
    void forget();

    [[nodiscard]] static int interval_seconds(const olagarro::oui::Settings& settings);

  private:
    // What the copy depends on: each unsaved document, its text's revision and its file.
    using Fingerprint = std::vector<std::tuple<const olagarro::oui::TextEdit*, std::size_t, std::string>>;

    [[nodiscard]] Fingerprint fingerprint() const;
    void tick();

    olagarro::oui::Application& m_application;
    olagarro::oui::Settings& m_settings;
    const Editors& m_editors;
    olagarro::oui::Timer m_timer;
    // What the last copy held; nothing at first, so a start with nothing unsaved writes nothing.
    Fingerprint m_kept;
};
