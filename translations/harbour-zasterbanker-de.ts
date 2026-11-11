<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="de">
<context>
    <name>AboutPage</name>
    <message>
        <location filename="../qml/pages/AboutPage.qml" line="40"/>
        <source>About Zaster Banker</source>
        <translation>Über Zaster Banker</translation>
    </message>
    <message>
        <location filename="../qml/pages/AboutPage.qml" line="58"/>
        <source>Zaster Banker %1</source>
        <translation>Zaster Banker %1</translation>
    </message>
    <message>
        <location filename="../qml/pages/AboutPage.qml" line="64"/>
        <source>Personal finances for Sailfish OS</source>
        <translation>Persönliche Finanzen für Sailfish OS</translation>
    </message>
    <message>
        <location filename="../qml/pages/AboutPage.qml" line="72"/>
        <source>By Sebastian J. Wolf and Claude</source>
        <translation>Von Sebastian J. Wolf und Claude</translation>
    </message>
    <message>
        <location filename="../qml/pages/AboutPage.qml" line="77"/>
        <source>Send e-mail</source>
        <translation>E-Mail senden</translation>
    </message>
    <message>
        <location filename="../qml/pages/AboutPage.qml" line="89"/>
        <source>Licensed under GNU GPLv3 or later</source>
        <translation>Lizenziert unter GNU GPLv3 oder später</translation>
    </message>
    <message>
        <location filename="../qml/pages/AboutPage.qml" line="94"/>
        <source>Sources on GitHub</source>
        <translation>Quellcode auf GitHub</translation>
    </message>
    <message>
        <location filename="../qml/pages/AboutPage.qml" line="97"/>
        <source>Your data</source>
        <translation>Ihre Daten</translation>
    </message>
    <message>
        <location filename="../qml/pages/AboutPage.qml" line="100"/>
        <source>Zaster Banker talks to your bank directly, via FinTS, and to nobody else. There is no server in between and no tracking.</source>
        <translation>Zaster Banker spricht über FinTS direkt mit Ihrer Bank und mit niemandem sonst. Es gibt keinen Server dazwischen und kein Tracking.</translation>
    </message>
    <message>
        <location filename="../qml/pages/AboutPage.qml" line="104"/>
        <source>What it keeps - your bank accesses, login names, accounts and their last balances - is stored encrypted with AES-256. The key lives in Sailfish Secrets and is unlocked together with your device lock.</source>
        <translation>Was die App speichert - Ihre Bankzugänge, Anmeldenamen, Konten und deren letzte Kontostände - wird mit AES-256 verschlüsselt. Der Schlüssel liegt in Sailfish Secrets und wird zusammen mit Ihrer Gerätesperre entsperrt.</translation>
    </message>
    <message>
        <location filename="../qml/pages/AboutPage.qml" line="108"/>
        <source>Your PIN is never stored. Once the bank has accepted it, it is kept in the app&apos;s memory for 5 minutes after your last action with that bank access, and forgotten when the app ends. Transactions and holdings are kept in memory only.</source>
        <translation>Ihre PIN wird nie gespeichert. Sobald die Bank sie akzeptiert hat, behält die App sie für 5 Minuten nach Ihrer letzten Aktion mit diesem Bankzugang im Arbeitsspeicher und vergisst sie, wenn sie beendet wird. Umsätze und Depotbestände werden nur im Arbeitsspeicher gehalten.</translation>
    </message>
    <message>
        <location filename="../qml/pages/AboutPage.qml" line="111"/>
        <source>Credits</source>
        <translation>Danksagungen</translation>
    </message>
    <message>
        <location filename="../qml/pages/AboutPage.qml" line="114"/>
        <source>FinTS is spoken by AqBanking and Gwenhywfar by Martin Preuß and contributors. Thanks for making them available under the conditions of the GNU GPL and the GNU LGPL!</source>
        <translation>FinTS spricht Zaster Banker mit AqBanking und Gwenhywfar von Martin Preuß und Mitwirkenden. Vielen Dank, dass sie unter den Bedingungen der GNU GPL und der GNU LGPL verfügbar sind!</translation>
    </message>
    <message>
        <location filename="../qml/pages/AboutPage.qml" line="119"/>
        <source>Open the AqBanking website</source>
        <translation>Website von AqBanking öffnen</translation>
    </message>
    <message>
        <location filename="../qml/pages/AboutPage.qml" line="123"/>
        <source>Secure connections use GnuTLS, Nettle and GMP; encryption uses libgcrypt and libgpg-error. Thanks to their authors for making them available under the conditions of the GNU LGPL!</source>
        <translation>Sichere Verbindungen nutzen GnuTLS, Nettle und GMP, die Verschlüsselung libgcrypt und libgpg-error. Vielen Dank an ihre Autoren, dass sie unter den Bedingungen der GNU LGPL verfügbar sind!</translation>
    </message>
    <message>
        <location filename="../qml/pages/AboutPage.qml" line="127"/>
        <source>The list of banks is the FinTS institute list of the Deutsche Kreditwirtschaft.</source>
        <translation>Die Liste der Banken ist die FinTS-Institutsliste der Deutschen Kreditwirtschaft.</translation>
    </message>
    <message>
        <location filename="../qml/pages/AboutPage.qml" line="131"/>
        <source>Zaster Banker was written by its author together with Claude, Anthropic&apos;s assistant, in Claude Code. The commits it worked on name it as a co-author.</source>
        <translation>Zaster Banker wurde von seinem Autor zusammen mit Claude, dem Assistenten von Anthropic, in Claude Code geschrieben. Die Commits, an denen Claude mitgearbeitet hat, nennen es als Co-Autor.</translation>
    </message>
    <message>
        <location filename="../qml/pages/AboutPage.qml" line="136"/>
        <source>Open the Claude Code website</source>
        <translation>Website von Claude Code öffnen</translation>
    </message>
</context>
<context>
    <name>BankMessagesPage</name>
    <message>
        <location filename="../qml/pages/BankMessagesPage.qml" line="48"/>
        <source>Messages from the bank</source>
        <translation>Meldungen der Bank</translation>
    </message>
    <message>
        <location filename="../qml/pages/BankMessagesPage.qml" line="58"/>
        <source>Each line is a result code with the bank&apos;s text, from 0 (done) over 3 (a note or warning) to 9 (an error). S: about part of the request, M: about the whole of it.</source>
        <translation>Jede Zeile ist ein Rückmeldungscode mit dem Text der Bank, von 0 (ausgeführt) über 3 (Hinweis oder Warnung) bis 9 (Fehler). S: zu einem Teil des Auftrags, M: zum ganzen Auftrag.</translation>
    </message>
    <message>
        <location filename="../qml/pages/BankMessagesPage.qml" line="69"/>
        <source>Copy to clipboard</source>
        <translation>In die Zwischenablage kopieren</translation>
    </message>
</context>
<context>
    <name>BankTotal</name>
    <message>
        <location filename="../qml/components/BankTotal.qml" line="53"/>
        <source>Total balance</source>
        <translation>Gesamtsumme</translation>
    </message>
    <message>
        <location filename="../qml/components/BankTotal.qml" line="31"/>
        <source>Not all accounts included</source>
        <translation>Nicht alle Konten enthalten</translation>
    </message>
</context>
<context>
    <name>Banking</name>
    <message>
        <location filename="../src/Banking.cpp" line="206"/>
        <source>Your data was deleted, but its key could not be removed from Sailfish Secrets: %1</source>
        <translation>Ihre Daten wurden gelöscht, aber ihr Schlüssel konnte nicht aus Sailfish Secrets entfernt werden: %1</translation>
    </message>
</context>
<context>
    <name>BankingThread</name>
    <message>
        <location filename="../src/BankingThread.cpp" line="387"/>
        <source>Your stored data cannot be decrypted.</source>
        <translation>Ihre gespeicherten Daten können nicht entschlüsselt werden.</translation>
    </message>
    <message>
        <location filename="../src/BankingThread.cpp" line="402"/>
        <source>The banking library could not be started (%1).</source>
        <translation>Die Banking-Bibliothek konnte nicht gestartet werden (%1).</translation>
    </message>
    <message>
        <location filename="../src/BankingThread.cpp" line="415"/>
        <location filename="../src/BankingThread.cpp" line="548"/>
        <location filename="../src/BankingThread.cpp" line="610"/>
        <location filename="../src/BankingThread.cpp" line="705"/>
        <location filename="../src/BankingThread.cpp" line="751"/>
        <source>Banking is not available.</source>
        <translation>Das Banking ist nicht verfügbar.</translation>
    </message>
    <message>
        <location filename="../src/BankingThread.cpp" line="429"/>
        <source>The bank access could not be created.</source>
        <translation>Der Bankzugang konnte nicht angelegt werden.</translation>
    </message>
    <message>
        <location filename="../src/BankingThread.cpp" line="434"/>
        <source>Connecting to the bank...</source>
        <translation>Verbinde mit der Bank...</translation>
    </message>
    <message>
        <location filename="../src/BankingThread.cpp" line="437"/>
        <source>The bank did not accept the connection.</source>
        <translation>Die Bank hat die Verbindung nicht angenommen.</translation>
    </message>
    <message>
        <location filename="../src/BankingThread.cpp" line="450"/>
        <source>Retrieving your accounts...</source>
        <translation>Rufe Ihre Konten ab...</translation>
    </message>
    <message>
        <location filename="../src/BankingThread.cpp" line="453"/>
        <source>The bank did not list your accounts.</source>
        <translation>Die Bank hat Ihre Konten nicht übermittelt.</translation>
    </message>
    <message>
        <location filename="../src/BankingThread.cpp" line="490"/>
        <source>TAN method</source>
        <translation>TAN-Verfahren</translation>
    </message>
    <message>
        <location filename="../src/BankingThread.cpp" line="491"/>
        <source>Your bank offers several ways to confirm with a TAN. Which one do you use?</source>
        <translation>Ihre Bank bietet mehrere Verfahren an, um mit einer TAN zu bestätigen. Welches nutzen Sie?</translation>
    </message>
    <message>
        <location filename="../src/BankingThread.cpp" line="516"/>
        <source>TAN medium</source>
        <translation>TAN-Medium</translation>
    </message>
    <message>
        <location filename="../src/BankingThread.cpp" line="517"/>
        <source>Your bank needs the name of the device or app you confirm with. You find it in the security settings of your online banking or of the bank&apos;s app, e.g. under &quot;Sicherheitsverfahren&quot; for BestSign. Enter it exactly as shown there, capitals included.</source>
        <translation>Ihre Bank braucht den Namen des Geräts oder der App, mit der Sie freigeben. Sie finden ihn in den Sicherheitseinstellungen Ihres Online-Bankings oder der App Ihrer Bank, bei BestSign z. B. unter „Sicherheitsverfahren“. Geben Sie ihn genau so ein, wie er dort steht, auch die Groß- und Kleinschreibung.</translation>
    </message>
    <message>
        <location filename="../src/BankingThread.cpp" line="526"/>
        <source>The name can have up to %1 characters.</source>
        <translation>Der Name darf bis zu %1 Zeichen lang sein.</translation>
    </message>
    <message>
        <location filename="../src/BankingThread.cpp" line="532"/>
        <source>The name contains a character that cannot be sent to the bank.</source>
        <translation>Der Name enthält ein Zeichen, das nicht an die Bank übertragen werden kann.</translation>
    </message>
    <message>
        <location filename="../src/BankingThread.cpp" line="563"/>
        <location filename="../src/BankingThread.cpp" line="583"/>
        <source>There are no accounts.</source>
        <translation>Es gibt keine Konten.</translation>
    </message>
    <message>
        <location filename="../src/BankingThread.cpp" line="588"/>
        <source>Retrieving balances...</source>
        <translation>Rufe Kontostände ab...</translation>
    </message>
    <message>
        <location filename="../src/BankingThread.cpp" line="627"/>
        <source>Retrieving transactions...</source>
        <translation>Rufe Umsätze ab...</translation>
    </message>
    <message>
        <location filename="../src/BankingThread.cpp" line="662"/>
        <source>The transactions could not be retrieved.</source>
        <translation>Die Umsätze konnten nicht abgerufen werden.</translation>
    </message>
    <message>
        <location filename="../src/BankingThread.cpp" line="659"/>
        <source>The bank&apos;s answer could not be read.</source>
        <translation>Die Antwort der Bank konnte nicht gelesen werden.</translation>
    </message>
    <message>
        <location filename="../src/BankingThread.cpp" line="660"/>
        <source>The bank does not offer transactions for this account.</source>
        <translation>Die Bank bietet für dieses Konto keine Umsätze an.</translation>
    </message>
    <message>
        <location filename="../src/BankingThread.cpp" line="601"/>
        <source>The bank sent no balances.</source>
        <translation>Die Bank hat keine Kontostände übermittelt.</translation>
    </message>
    <message>
        <location filename="../src/BankingThread.cpp" line="710"/>
        <source>Retrieving holdings...</source>
        <translation>Rufe Depotbestand ab...</translation>
    </message>
    <message>
        <location filename="../src/BankingThread.cpp" line="741"/>
        <source>The bank does not offer the holdings of this account.</source>
        <translation>Die Bank bietet den Bestand dieses Depots nicht an.</translation>
    </message>
    <message>
        <location filename="../src/BankingThread.cpp" line="742"/>
        <source>The holdings could not be retrieved.</source>
        <translation>Der Depotbestand konnte nicht abgerufen werden.</translation>
    </message>
    <message>
        <location filename="../src/BankingThread.cpp" line="756"/>
        <source>The bank access could not be removed.</source>
        <translation>Der Bankzugang konnte nicht entfernt werden.</translation>
    </message>
    <message>
        <location filename="../src/BankingThread.cpp" line="766"/>
        <source>The stored data could not be removed.</source>
        <translation>Die gespeicherten Daten konnten nicht entfernt werden.</translation>
    </message>
    <message>
        <location filename="../src/BankingThread.cpp" line="909"/>
        <source>Cancelled.</source>
        <translation>Abgebrochen.</translation>
    </message>
    <message>
        <location filename="../src/BankingThread.cpp" line="914"/>
        <source>%1 (Error %2)</source>
        <translation>%1 (Fehler %2)</translation>
    </message>
</context>
<context>
    <name>CoverPage</name>
    <message>
        <location filename="../qml/cover/CoverPage.qml" line="112"/>
        <source>Not all accounts</source>
        <translation>Nicht alle Konten</translation>
    </message>
</context>
<context>
    <name>CredentialsDialog</name>
    <message>
        <location filename="../qml/pages/CredentialsDialog.qml" line="48"/>
        <source>Connect</source>
        <translation>Verbinden</translation>
    </message>
    <message>
        <location filename="../qml/pages/CredentialsDialog.qml" line="63"/>
        <source>%1 · Bank code %2</source>
        <translation>%1 · BLZ %2</translation>
    </message>
    <message>
        <location filename="../qml/pages/CredentialsDialog.qml" line="64"/>
        <source>Bank code %1</source>
        <translation>BLZ %1</translation>
    </message>
    <message>
        <location filename="../qml/pages/CredentialsDialog.qml" line="73"/>
        <source>Login name or user ID</source>
        <translation>Anmeldename oder Benutzerkennung</translation>
    </message>
    <message>
        <location filename="../qml/pages/CredentialsDialog.qml" line="83"/>
        <source>Customer ID, if your bank gave you one</source>
        <translation>Kunden-ID, falls Ihre Bank Ihnen eine gegeben hat</translation>
    </message>
    <message>
        <location filename="../qml/pages/CredentialsDialog.qml" line="84"/>
        <source>Customer ID (optional)</source>
        <translation>Kunden-ID (optional)</translation>
    </message>
    <message>
        <location filename="../qml/pages/CredentialsDialog.qml" line="97"/>
        <source>Use the same login as for your bank&apos;s online banking. Your PIN is asked for when connecting and is never stored.</source>
        <translation>Verwenden Sie dieselbe Anmeldung wie für das Online-Banking Ihrer Bank. Ihre PIN wird beim Verbinden abgefragt und nie gespeichert.</translation>
    </message>
</context>
<context>
    <name>DepotPage</name>
    <message>
        <location filename="../qml/pages/DepotPage.qml" line="83"/>
        <source>Not all holdings included</source>
        <translation>Nicht alle Positionen enthalten</translation>
    </message>
    <message>
        <location filename="../qml/pages/DepotPage.qml" line="90"/>
        <source>Refresh holdings</source>
        <translation>Depotbestand aktualisieren</translation>
    </message>
    <message>
        <location filename="../qml/pages/DepotPage.qml" line="123"/>
        <source>%1 × %2</source>
        <translation>%1 × %2</translation>
    </message>
    <message>
        <location filename="../qml/pages/DepotPage.qml" line="165"/>
        <source>No holdings</source>
        <translation>Keine Wertpapiere</translation>
    </message>
    <message>
        <location filename="../qml/pages/DepotPage.qml" line="166"/>
        <source>The bank reports no securities in this depot.</source>
        <translation>Die Bank meldet keine Wertpapiere in diesem Depot.</translation>
    </message>
    <message>
        <location filename="../qml/pages/DepotPage.qml" line="171"/>
        <source>Holdings unavailable</source>
        <translation>Depotbestand nicht verfügbar</translation>
    </message>
</context>
<context>
    <name>GwenGui</name>
    <message>
        <location filename="../src/GwenGui.cpp" line="206"/>
        <source>The TAN contains a character that cannot be sent to the bank.</source>
        <translation>Die TAN enthält ein Zeichen, das nicht an die Bank übertragen werden kann.</translation>
    </message>
    <message>
        <location filename="../src/GwenGui.cpp" line="207"/>
        <source>The PIN contains a character that cannot be sent to the bank.</source>
        <translation>Die PIN enthält ein Zeichen, das nicht an die Bank übertragen werden kann.</translation>
    </message>
    <message>
        <location filename="../src/GwenGui.cpp" line="212"/>
        <source>The TAN has an invalid length.</source>
        <translation>Die TAN hat eine ungültige Länge.</translation>
    </message>
    <message>
        <location filename="../src/GwenGui.cpp" line="213"/>
        <source>The PIN has an invalid length.</source>
        <translation>Die PIN hat eine ungültige Länge.</translation>
    </message>
    <message>
        <location filename="../src/GwenGui.cpp" line="262"/>
        <source>OK</source>
        <translation>OK</translation>
    </message>
    <message>
        <location filename="../src/GwenGui.cpp" line="349"/>
        <source>The bank&apos;s server could not prove its identity, the connection was refused. %1</source>
        <translation>Der Server der Bank konnte seine Identität nicht nachweisen, die Verbindung wurde abgelehnt. %1</translation>
    </message>
</context>
<context>
    <name>InstituteSearchPage</name>
    <message>
        <location filename="../qml/pages/InstituteSearchPage.qml" line="38"/>
        <source>Bank code %1</source>
        <translation>BLZ %1</translation>
    </message>
    <message>
        <location filename="../qml/pages/InstituteSearchPage.qml" line="59"/>
        <source>Choose your bank</source>
        <translation>Bank auswählen</translation>
    </message>
    <message>
        <location filename="../qml/pages/InstituteSearchPage.qml" line="65"/>
        <source>Name, city, bank code or BIC</source>
        <translation>Name, Ort, BLZ oder BIC</translation>
    </message>
    <message>
        <location filename="../qml/pages/InstituteSearchPage.qml" line="142"/>
        <source>No bank found</source>
        <translation>Keine Bank gefunden</translation>
    </message>
    <message>
        <location filename="../qml/pages/InstituteSearchPage.qml" line="143"/>
        <source>Only banks offering FinTS with PIN/TAN are listed.</source>
        <translation>Aufgeführt sind nur Banken, die FinTS mit PIN/TAN anbieten.</translation>
    </message>
</context>
<context>
    <name>KeyStore</name>
    <message>
        <location filename="../src/KeyStore.cpp" line="109"/>
        <source>Sailfish Secrets is not available: %1</source>
        <translation>Sailfish Secrets ist nicht verfügbar: %1</translation>
    </message>
    <message>
        <location filename="../src/KeyStore.cpp" line="119"/>
        <source>The key to your stored data is gone, the data cannot be decrypted anymore.</source>
        <translation>Der Schlüssel zu Ihren gespeicherten Daten fehlt, die Daten können nicht mehr entschlüsselt werden.</translation>
    </message>
    <message>
        <location filename="../src/KeyStore.cpp" line="144"/>
        <source>The key to your stored data is damaged.</source>
        <translation>Der Schlüssel zu Ihren gespeicherten Daten ist beschädigt.</translation>
    </message>
    <message>
        <location filename="../src/KeyStore.cpp" line="146"/>
        <source>After every restart of the device, Sailfish OS asks whether Zaster Banker may read the key to your stored data. This time it was not confirmed (%1).</source>
        <translation>Nach jedem Neustart des Geräts fragt Sailfish OS, ob Zaster Banker den Schlüssel zu Ihren gespeicherten Daten lesen darf. Diesmal wurde das nicht bestätigt (%1).</translation>
    </message>
    <message>
        <location filename="../src/KeyStore.cpp" line="149"/>
        <source>The key to your stored data could not be read: %1</source>
        <translation>Der Schlüssel zu Ihren gespeicherten Daten konnte nicht gelesen werden: %1</translation>
    </message>
    <message>
        <location filename="../src/KeyStore.cpp" line="155"/>
        <source>The key store could not be created: %1</source>
        <translation>Der Schlüsselspeicher konnte nicht angelegt werden: %1</translation>
    </message>
    <message>
        <location filename="../src/KeyStore.cpp" line="164"/>
        <source>The key could not be stored: %1</source>
        <translation>Der Schlüssel konnte nicht gespeichert werden: %1</translation>
    </message>
    <message>
        <location filename="../src/KeyStore.cpp" line="179"/>
        <source>Sailfish OS asks to confirm the removal of the key, and it was not confirmed (%1).</source>
        <translation>Sailfish OS fragt nach einer Bestätigung, bevor der Schlüssel entfernt wird, und sie wurde nicht gegeben (%1).</translation>
    </message>
</context>
<context>
    <name>MessageDialog</name>
    <message>
        <location filename="../qml/pages/MessageDialog.qml" line="78"/>
        <source>OK</source>
        <translation>OK</translation>
    </message>
</context>
<context>
    <name>OverviewPage</name>
    <message>
        <location filename="../qml/pages/OverviewPage.qml" line="67"/>
        <source>About Zaster Banker</source>
        <translation>Über Zaster Banker</translation>
    </message>
    <message>
        <location filename="../qml/pages/OverviewPage.qml" line="71"/>
        <source>Delete all data</source>
        <translation>Alle Daten löschen</translation>
    </message>
    <message>
        <location filename="../qml/pages/OverviewPage.qml" line="73"/>
        <source>Deleting all data</source>
        <translation>Lösche alle Daten</translation>
    </message>
    <message>
        <location filename="../qml/pages/OverviewPage.qml" line="76"/>
        <source>Try again</source>
        <translation>Erneut versuchen</translation>
    </message>
    <message>
        <location filename="../qml/pages/OverviewPage.qml" line="82"/>
        <source>Add bank access</source>
        <translation>Bankzugang hinzufügen</translation>
    </message>
    <message>
        <location filename="../qml/pages/OverviewPage.qml" line="88"/>
        <location filename="../qml/pages/OverviewPage.qml" line="113"/>
        <source>Refresh balances</source>
        <translation>Kontostände aktualisieren</translation>
    </message>
    <message>
        <location filename="../qml/pages/OverviewPage.qml" line="118"/>
        <source>Remove this bank access</source>
        <translation>Diesen Bankzugang entfernen</translation>
    </message>
    <message>
        <location filename="../qml/pages/OverviewPage.qml" line="122"/>
        <source>Removing %1</source>
        <translation>Entferne %1</translation>
    </message>
    <message>
        <location filename="../qml/pages/OverviewPage.qml" line="176"/>
        <source>Account %1</source>
        <translation>Konto %1</translation>
    </message>
    <message>
        <location filename="../qml/pages/OverviewPage.qml" line="227"/>
        <source>No bank access yet</source>
        <translation>Noch kein Bankzugang</translation>
    </message>
    <message>
        <location filename="../qml/pages/OverviewPage.qml" line="228"/>
        <source>Pull down to add your bank</source>
        <translation>Herunterziehen, um Ihre Bank hinzuzufügen</translation>
    </message>
    <message>
        <location filename="../qml/pages/OverviewPage.qml" line="233"/>
        <source>Stored data unreadable</source>
        <translation>Gespeicherte Daten unlesbar</translation>
    </message>
    <message>
        <location filename="../qml/pages/OverviewPage.qml" line="233"/>
        <source>Banking unavailable</source>
        <translation>Banking nicht verfügbar</translation>
    </message>
    <message>
        <location filename="../qml/pages/OverviewPage.qml" line="235"/>
        <source>Pull down to delete all data and start afresh.</source>
        <translation>Herunterziehen, um alle Daten zu löschen und neu zu beginnen.</translation>
    </message>
    <message>
        <location filename="../qml/pages/OverviewPage.qml" line="236"/>
        <source>Pull down to try again, or to delete all data and start afresh.</source>
        <translation>Herunterziehen, um es erneut zu versuchen oder um alle Daten zu löschen und neu zu beginnen.</translation>
    </message>
    <message>
        <location filename="../qml/pages/OverviewPage.qml" line="258"/>
        <source>Unlocking your data...</source>
        <translation>Entsperre Ihre Daten...</translation>
    </message>
</context>
<context>
    <name>PinDialog</name>
    <message>
        <location filename="../qml/pages/PinDialog.qml" line="76"/>
        <location filename="../qml/pages/PinDialog.qml" line="95"/>
        <source>PIN</source>
        <translation>PIN</translation>
    </message>
    <message>
        <location filename="../qml/pages/PinDialog.qml" line="109"/>
        <source>Once the bank accepts it, the PIN is remembered for 5 minutes after your last action with this bank access, as long as the app runs.</source>
        <translation>Sobald die Bank sie akzeptiert hat, merkt sich die App die PIN für 5 Minuten nach Ihrer letzten Aktion mit diesem Bankzugang, solange sie läuft.</translation>
    </message>
</context>
<context>
    <name>SetupPage</name>
    <message>
        <location filename="../qml/pages/SetupPage.qml" line="49"/>
        <source>Your bank access is set up.</source>
        <translation>Ihr Bankzugang ist eingerichtet.</translation>
    </message>
    <message>
        <location filename="../qml/pages/SetupPage.qml" line="64"/>
        <source>Connected</source>
        <translation>Verbunden</translation>
    </message>
    <message>
        <location filename="../qml/pages/SetupPage.qml" line="64"/>
        <source>Not connected</source>
        <translation>Nicht verbunden</translation>
    </message>
    <message>
        <location filename="../qml/pages/SetupPage.qml" line="65"/>
        <source>Connecting</source>
        <translation>Verbinde</translation>
    </message>
    <message>
        <location filename="../qml/pages/SetupPage.qml" line="87"/>
        <source>Done</source>
        <translation>Fertig</translation>
    </message>
    <message>
        <location filename="../qml/pages/SetupPage.qml" line="87"/>
        <source>Cancel</source>
        <translation>Abbrechen</translation>
    </message>
</context>
<context>
    <name>TanDialog</name>
    <message>
        <location filename="../qml/pages/TanDialog.qml" line="73"/>
        <location filename="../qml/pages/TanDialog.qml" line="117"/>
        <source>TAN</source>
        <translation>TAN</translation>
    </message>
</context>
<context>
    <name>TransactionsPage</name>
    <message>
        <location filename="../qml/pages/TransactionsPage.qml" line="111"/>
        <source>Refresh transactions</source>
        <translation>Umsätze aktualisieren</translation>
    </message>
    <message>
        <location filename="../qml/pages/TransactionsPage.qml" line="56"/>
        <location filename="../qml/pages/TransactionsPage.qml" line="246"/>
        <source>Pending</source>
        <translation>Vorgemerkt</translation>
    </message>
    <message>
        <location filename="../qml/pages/TransactionsPage.qml" line="46"/>
        <source>Pull down for the bank&apos;s messages.</source>
        <translation>Die Meldungen der Bank finden Sie im Menü oben.</translation>
    </message>
    <message>
        <location filename="../qml/pages/TransactionsPage.qml" line="103"/>
        <source>Messages from the bank</source>
        <translation>Meldungen der Bank</translation>
    </message>
    <message>
        <location filename="../qml/pages/TransactionsPage.qml" line="135"/>
        <source>Copy to clipboard</source>
        <translation>In die Zwischenablage kopieren</translation>
    </message>
    <message>
        <location filename="../qml/pages/TransactionsPage.qml" line="255"/>
        <source>No transactions</source>
        <translation>Keine Umsätze</translation>
    </message>
    <message>
        <location filename="../qml/pages/TransactionsPage.qml" line="256"/>
        <source>The bank reports none for the last 90 days.</source>
        <translation>Die Bank meldet für die letzten 90 Tage keine.</translation>
    </message>
    <message>
        <location filename="../qml/pages/TransactionsPage.qml" line="262"/>
        <source>Transactions unavailable</source>
        <translation>Umsätze nicht verfügbar</translation>
    </message>
</context>
</TS>
