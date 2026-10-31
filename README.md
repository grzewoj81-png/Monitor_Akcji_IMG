# ESP32-2432S028 — zegar internetowy

Prosty zegar z czasem pobieranym z serwerow NTP. Interfejs dotykowy pozwala wybrac widoczna siec WiFi, wpisac haslo ekranowa
klawiatura i zapamietac polaczenie w pamieci ESP32. Przy pierwszym uruchomieniu
automatycznie uruchamia sie kalibracja dotyku, a jej wynik jest zapisywany w
pamieci. Ostatnia siec jest wybierana automatycznie po ponownym uruchomieniu.
Czas jest wyswietlany w strefie Europe/Warsaw z automatyczna zmiana czasu
letniego i zimowego.

Na ekranie glownym zegar i data sa wyswietlane u gory, a ponizej znajduje sie
wykres kursu IMAGE POWER z ostatniej sesji 9:00-17:00, rozciagniety prawie na
cala szerokosc i wysokosc ekranu. Kurs jest pokazany bardzo duzymi cyframi z
dwoma miejscami po przecinku na tle wykresu; dioda RGB swieci w kolorze kursu
(zielony wzrost, czerwony spadek, szary brak zmiany lub danych). Diode mozna
wlaczyc lub wylaczyc przelacznikiem **Dioda RGB kursu** w opcjach lub panelu WWW.
Kurs i zmiana procentowa sa pobierane raz na minute z Yahoo Finance
(symbol IPW.WA, odnaleziony po ISIN PLIMGPW00013);
widoczne punkty odpowiadaja pieciominutowym przedzialom, w ktorych zawarto
transakcje. Wiek kursu na ekranie oznacza czas od ostatniej transakcji Yahoo,
a nie gwarantowane opoznienie dostarczenia danych. Historia wykresu jest
przechowywana tylko w pamieci RAM i nie jest zapisywana we flash. Yahoo wymaga
polaczenia z WiFi oraz ustawionego czasu, potrzebnego do bezpiecznej weryfikacji
HTTPS.

Urzadzenie domyslnie uruchamia tez zabezpieczony punkt dostepowy
`MonitorAkcji-XXXXXX`; mozna go wlaczyc lub wylaczyc w opcjach. Wylaczenie
hotspotu nie rozlacza urzadzenia od routera Wi-Fi. Przy pierwszym uruchomieniu
urzadzenie losuje unikalne haslo punktu dostepowego i panelu WWW oraz zapisuje
je w pamieci NVS. Haslo jest wyswietlane na ekranie urzadzenia; nazwa
uzytkownika panelu to `admin`. Po polaczeniu z wlaczonym punktem dostepowym
telefon powinien otworzyc strone konfiguracji sieci. Jesli tak sie nie stanie,
otworz `http://192.168.4.1` i wybierz **Otworz panel konfiguracji**. Zaloguj
sie nazwa `admin` i haslem pokazanym na ekranie. Z sieci routera otworz adres IP
urzadzenia pokazany na ekranie WiFi/IP i wybierz panel konfiguracji (lub
dopisz `/login` do adresu). Webowy panel pozwala skanowac i wybierac sieci,
ustawic czas oraz zresetowac kalibracje dotyku. Automatyczne okno zalezy od
systemu telefonu; adres mozna zawsze wpisac recznie.

## Obsluga

- Ikona trybika w prawym gornym rogu otwiera ustawienia.
- Ikona zasiegu WiFi pokazuje stan i sile polaczenia; dotknij jej, aby otworzyc
  ekran WiFi / IP.
- Menu opcji przewija sie plynnie za palcem, a po szybkim przesunieciu
  wyhamowuje z bezwladnoscia.
- **Synchronizacja czasu** i **Punkt dostepowy** sa przelacznikami w opcjach;
  zmiany sa zapisywane po ponownym uruchomieniu.
- **Jasnosc ekranu** pozwala ustawic reczny poziom od 1% do 100% oraz
  wlaczyc automatyke GPIO34. Regulowane suwakiem minimalny i maksymalny poziom
  automatyki ograniczaja jej zakres; czulosc ustala, jak szybko jasnosc rosnie
  wraz z oswietleniem. Odczyt 1117 odpowiada minimalnemu podswietleniu, a
  nizszy odczyt oznacza wiecej swiatla. Surowy odczyt jest widoczny na ekranie
  regulacji. Przesuniecie suwaka poziomu recznego wylacza automat.
  Poziom, zakres, czulosc i stan automatyki sa zapamietywane.
- **Karta SD** pokazuje wykrycie karty oraz jej pojemnosc i zajete miejsce.
  Przycisk **FORMATUJ FAT32** wymaga dodatkowego potwierdzenia i usuwa
  wszystkie pliki. Gniazdo korzysta z SPI: CS GPIO5, SCK GPIO18, MISO GPIO19
  i MOSI GPIO23; podczas sprawdzania karty piny SPI dotyku sa przywracane.
  Uzywaj karty sformatowanej jako FAT/FAT32. Zdarzenia menu, polaczen WiFi,
  regulacji, karty SD i bledy pracy sa dopisywane z data i godzina do
  `/log.txt`; plik ma kopię rotacyjną `/log.bak`. Istniejący
  `/monitorakcji.txt` zostanie przemianowany przy pierwszym dopisaniu wpisu.
  Przycisk
  **PODGLĄD LOGU** pokazuje do 30 najnowszych wpisów na ekranie, po 15
  skróconych wierszy na stronę z mniejszymi przyciskami **STARSZE/NOWSZE**.
  Panel WWW wyświetla do 30 najnowszych wpisów w przewijanym polu i pozwala je
  odświeżyć; na komputerze układ wykorzystuje dwie kolumny.
  Udane pobrania kursu IMAGE POWER są zapisywane co około minutę w
  `/imagepower.csv` (kolumny: czas notowania, cena PLN, zmiana procentowa);
  po 256 KiB plik jest obracany do `/imagepower.bak`.
- **WYBIERZ SIEC WIFI** w opcjach: wyszukuje dostepne sieci; animowany pasek
  pokazuje trwajace skanowanie, a po jego zakonczeniu liczbe znalezionych sieci.
  Jesli zegar jest polaczony z routerem, polaczenie zostanie wznowione po skanie.
- Dotknij nazwy sieci, aby sie polaczyc; przy zapisanej sieci przycisk
  **USUN** usuwa jej haslo i wylacza automatyczne laczenie.
- Niebieska pozycja to nowa siec; dotknij jej, wpisz haslo na klawiaturze
  ekranowej i wybierz **OK**. Haslo zostanie zapisane po udanym polaczeniu.
- **KALIBRACJA DOTYKU** w opcjach pozwala ponownie skalibrowac ekran.
- **Konfiguracja WiFi / IP** otwiera osobny ekran z SSID i adresem IP routera
  oraz informacja o punkcie dostepowym.
- Kalibracje mozna zresetowac takze w panelu WWW; urzadzenie ponownie wyswietli
  punkty kalibracyjne.
- Po polaczeniu z WiFi otworz
  `http://<adres-IP>` w przegladarce, aby zobaczyc date i godzine lub ustawic
  je recznie. Ustawiony recznie czas jest zapisywany jako wartosc startowa;
  po polaczeniu z WiFi zegar uruchamia synchronizacje NTP.
- Panel WWW zawiera ustawienia NTP i punktu dostepowego, jasnosc reczna i
  automatyczna wraz z czuloscia oraz zakresem min./maks., status WiFi/IP,
  skanowanie i laczenie z siecia, reset kalibracji dotyku oraz status i
  formatowanie karty SD. Formatowanie wymaga potwierdzenia i usuwa cala jej
  zawartosc. Zdarzenia wykonywane z panelu WWW rowniez trafiaja do pliku logu
  na karcie SD. Panel dopasowuje rozmiar: na komputerze jest bardziej zwarty,
  a na telefonie zachowuje wieksze, czytelne elementy.
- Jesli kurs nie jest dostepny, ekran pokazuje powod, np. brak WiFi, brak
  ustawionego czasu lub blad polaczenia Yahoo. Gdy synchronizacja NTP jest
  wylaczona, ustaw czas recznie w panelu WWW, aby umozliwic weryfikacje HTTPS.
- Do pamieci ESP32 mozna zapisac do 12 sieci.

Zegar odswieza tylko zmienione wartosci, a wykres po pobraniu nowej historii
Yahoo. Lista opcji jest rysowana w buforze obrazu, by unikac migania podczas
przewijania.

Yahoo zwraca ostatnia sesje gieldowa; w dni wolne moze to byc poprzedni dzien
sesyjny. Dla malo plynnych akcji czesc pieciominutowych przedzialow nie zawiera
transakcji i nie ma w nich punktu na wykresie. Pobieranie historii korzysta z
publicznego endpointu Yahoo Finance, ktorego dostepnosc i limity moga sie zmienic.

## Wgranie

Otworz projekt w PlatformIO, podlacz plytke i wybierz **Upload**. Monitor portu
szeregowego 115200 baud pokazuje wynik polaczenia. Zegar ustawi sie po
polaczeniu z WiFi i odpowiedzi serwera NTP.

Projekt jest przygotowany dla ESP32-2432S028 z ekranem ILI9341 i rezystancyjnym
dotykiem XPT2046.
