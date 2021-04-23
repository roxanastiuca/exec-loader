Nume: STIUCA Roxana-Elena
Grupa: 335CB

# Tema 3 SO - Loader de executabile

### Organizare
Pe langa structura exec, programul mai pastreaza:
* un file descriptor pentru fisierul executabil deschis in mod read-only;
* o structura de tip sigaction pentru handler-ul vechi/default al semnalului
SIGSEGV.

Campul data dintr-un segment so_seg este folosit pentru a stoca un vector (is_mapped),
in care byte-ul i este 0 daca pagina i din segmentul respectiv nu a fost inca mapata,
respectiv 1 daca pagina a fost deja mapata.

### Implementare
Este implementat intreg enuntul.

#### so_init_loader
Seteaza o noua functie ca fiind rutina de tratare a semnalului SIGSEGV, pastrand
totodata o referinta catre rutine veche/default.

#### so_execute
Deschide fisierul in mod RDONLY, pentru a pastra un file descriptor.
Parseaza fisierul dat ca input cu ajutorul functiei so_parse_exec.
Pentru fiecare segment, calculeaza numarul de pagini din el si aloca
atatia byte in campul data. Initial toti bytes sunt 0.
Incepe executia programului, folosind so_start_exec.
Daca executia nu este intrerupta, la final se apeleaza si so_end_exec
care elibereaza demapeaza zonele de memorie mapate si elibereaza memoria
alocata in campul data din so_seg.

#### Page fault handler
Rutina care trateaza semnalul SIGSEGV este pagefault_handler.
* Intai, gaseste segmentul din care face parte adresa de memorie ce s-a incercat
a fi accesata. Pentru asta se compara addr cu vaddr si vaddr + mem_size al
fiecarui segment.
* Daca nu apartine niciunui segment, se ruleaza handlerul default.
* Extragem din campul data al segmentului vectorul is_mapped. Daca pagina accesata
apare deja ca fiind mapata (is_mapped[pagenum] == 1), atunci se incearca un acces
nepermis al memoriei si trebuie sa rulam handlerul default.
* Mapam o pagina din acel segment care sa cuprinda adresa accesata (adresa la care
mapam trebuie sa fie aliniata cu paginile).
* Marcam magina ca fiind mapata.

##### Cazuri speciale (Corner cases)
* file_size < mem_size si pagina este in afara zonei de memorie rezervata fisierului:
pagina va fi mapata cu MAP_ANONYMOUS.
* file_size < mem_size si pagina are o parte din bytes in afara zonei de memorie
rezervata fisierului: pagina va fi mapata, dar bytes din afara fisierul vor fi
zero-izati.

### Cum se compileaza si cum se ruleaza?
**Creare biblioteca dinamica**:
- Linux - make / make build;

### Git
https://github.com/roxanastiuca/exec-loader
