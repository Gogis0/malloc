#include "wrapper.h"
#include <stdio.h>
#include <string.h>

/* Kod funkcii my_alloc a my_free nahradte vlastnym. Nepouzivajte ziadne
 * globalne ani staticke premenne; jedina globalna pamat je dostupna pomocou
 * mread/mwrite/msize, ktorych popis najdete vo wrapper.h */

/* Ukazkovy kod zvladne naraz iba jedinu alokaciu. V 0-tom bajte pamate si
 * pamata, ci je pamat od 1 dalej volna alebo obsadena. 
 *
 * V pripade, ze je volna, volanie my_allloc skonci uspesne a vrati zaciatok
 * alokovanej RAM; my_free pri volnej mamati zlyha.
 *
 * Ak uz nejaka alokacia prebehla a v 0-tom bajte je nenulova hodnota. Nie je
 * mozne spravit dalsiu alokaciu, takze my_alloc musi zlyhat. my_free naopak
 * zbehnut moze a uvolni pamat.
 */

typedef struct block_info {
    int prev;
    int next;
    int size; /* velkost pamate bloku */
    int addr; /* zaciatok pamate bloku */
    int free; /* info ci je block volny */
} preamble;


preamble read_preamble(int pos) {
    int p_size = sizeof(preamble);
    char buffer[p_size];
    preamble p;

    for (int i = pos; i < pos + p_size; i++) {
        buffer[i - pos] = mread(i);
    }
    memcpy(&p, buffer, p_size);
    return p;
}


void write_preamble(preamble p) {
    int p_size = sizeof(preamble);
    char buffer[p_size];
    memcpy(buffer, &p, p_size);

    for (int i = p.addr - p_size; i < p.addr; i++) {
        mwrite(i, buffer[i - (p.addr - p_size)]);
    }
}


/**
 * Inicializacia pamate
 *
 * Zavola sa, v stave, ked sa zacina s prazdnou pamatou, ktora je inicializovana
 * na 0.
 */
void my_init(void) {
    int p_size = sizeof(preamble);
    int m_size = msize();

    /* zaciatok */
    preamble init;
    init.addr = p_size;
    init.size = m_size - 2*p_size;
    init.free = 1;
    init.next = m_size - p_size;
    init.prev = -1;

    /* koniec */
    preamble term;
    term.addr = m_size;
    term.size = 0;
    term.next = -1;
    term.prev = 0;
    term.free = 0;

    /* zapisem mantinely do pamate */
    write_preamble(init);
    write_preamble(term);
    fprintf(stderr, "%s\n", "INITIALIZATION FINISHED");
}

/**
 * Poziadavka na alokaciu 'size' pamate. 
 *
 * Ak sa pamat podari alokovat, navratova hodnota je adresou prveho bajtu
 * alokovaneho priestoru v RAM. Pokial pamat uz nie je mozne alokovat, funkcia
 * vracia FAIL.
 */
int my_alloc(unsigned int size) {
    int p_size = sizeof(preamble); 
    int m_size = msize();
    char buffer[p_size];

    /* ak ziadam vacsiu pamat ako mam dokopy */
    if (size > m_size - p_size) return FAIL;

    preamble act_block;
    int offset = 0, found_suitable_block = 0;
    do {
        /* nacitam preambulu */
        act_block = read_preamble(offset);
        /* fprintf(stderr, "%d\n", act_block.addr); */

        if (act_block.free && act_block.size >= size) {
            found_suitable_block = 1;
            break;
        }
        offset = act_block.next;
    } while (act_block.next != -1);

    /* ak som nenasiel vhodny blok, vratim FAIL */
    if (!found_suitable_block) return FAIL;

    /* nasiel som vhodne miesto, obsadim z neho kolko potrebujem */

    /* ak mi ostalo dost pamate vytvorim doplnujuci block */
    if (act_block.size - size > p_size) {
        preamble new_block;
        new_block.free = 1;
        new_block.prev = act_block.addr - p_size;
        new_block.next = act_block.next;
        new_block.addr = act_block.addr + size + p_size;
        write_preamble(new_block);
    }
   
    /* useknem stary block */
    act_block.free = 0;
    act_block.size = size;
    act_block.next = act_block.addr + size;
    write_preamble(act_block);

    /* vratim zaciatok alokovanej pamate */
    return act_block.addr;
}

/**
 * Poziadavka na uvolnenie alokovanej pamate na adrese 'addr'.
 *
 * Ak bola pamat zacinajuca na adrese 'addr' alokovana, my_free ju uvolni a
 * vrati OK. Ak je adresa 'addr' chybna (nezacina na nej ziadna alokovana
 * pamat), my_free vracia FAIL.
 */

int my_free(unsigned int addr) {
    int p_size = sizeof(preamble);
    char buffer[p_size];

    preamble act_block = read_preamble(addr - p_size);
    act_block.free = 1;

    /* ak je predchadzajuci block free, mergni */
    if (act_block.prev != -1) {
        preamble prev_block = read_preamble(act_block.prev);
        if (prev_block.free) {
            prev_block.next = act_block.next;
            prev_block.size += act_block.size + p_size;

            act_block = prev_block;

            preamble next_block = read_preamble(act_block.next);
            next_block.prev = act_block.addr - p_size;
            write_preamble(next_block);
        }
    }

    /* ak je nasledujuci block free, mergni */
    if (act_block.next != -1) {
        preamble next_block = read_preamble(act_block.next);
        if (next_block.free) {
            act_block.next = next_block.next;
            act_block.size += next_block.size + p_size;

            preamble after_next = read_preamble(next_block.next);
            after_next.prev = next_block.prev;
        }
    }

    /* zapis upraveny usek */
    write_preamble(act_block);
}
