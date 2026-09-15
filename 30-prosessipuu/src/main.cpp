#include <print>
// g++ main.cpp -std=c++23 -o main && ./main

/*
Tehtävänanto: Kerää kaikista /proc-tiedostojärjestelmässä näkyvistä prosesseista 
PID ja ParentPID tiedot. Tulosta puurakenne näistä suhteista.

Ohjelman perusperiaate lienee toimia vastaavasti kuin pstree, joka on kehitetty
alun perin shell-skriptinä 1990-luvulla:
    https://github.com/FredHucht/pstree/tree/main
*/

/*
Linkkejä:
https://gitlab.com/procps-ng/procps/
https://www.kernel.org/doc/html/v4.12/core-api/kernel-api.html
*/

/*
https://www.kernel.org/doc/html/latest/filesystems/proc.html

/proc on virtuaalinen muistissa sijaitseva tiedostojärjestelmä, josta voi 
tavanomaisin menetelmin lukea tietoa. Käyttöjärjestelmä taustalla luo lennosta
tarvittavan tiedon, joka vastaa esim käyttäjän tiedostonlukuoperaatioon.

Käyttöjärjestelmä ei välttämättä osaa etukäteen vastata esim kyselyyn, miten
suuri virtuaalinen tiedosto /proc:ssa on, joten sitä on parasta lukea virtana
esim. stringstreamin avulla niin kauan, kuin käyttöjärjestelmä tarjoaa uutta tietoa.

Käytännössä prosessilistauksen lukemiseen tarvitsee C opendir() readdir() closedir()
funktiokutsuja. 
1. Luetaan /proc :sta lista kaikista prosesseista.
2. Luetaan luupissa /proc/PID/{stat tai status} jokaisen prosessin parent id
3. Rakenetaan soveltuva tietorakenne josta voi tulostaa puun alkaen sen juuresta
4. Tulostetaan tietorakenne sopivassa muodossa formatoituna komentoriville/
   tiedostoon tms.
*/

int main()
{
    std::print("Hello c++");
    return 0;
}
