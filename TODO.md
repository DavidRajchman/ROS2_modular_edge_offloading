Informace pro OM z BS (aktualne neni hotova finalni verze API odkud brat data. DATA musi dorazit do OM)
 - Pridat do OM transparentni API module, ktery umozni algoritmu si requestnout data z jakehokoliv json based API. - API moduly budou stylem handleru v MGW, v Algoritmu bude objekt ktery se inicializuje a nasledne da k dispozici funkce pro requestovani dat. Data se ulozi do struktury, ktera bude aktualivana na zaklade zadosti o refresh
- Stejne API pro informace o vytizenosti HW na kterem bezi MEC kontejnery

Popsat do global configu informace vykonu jedntlivych HW serveru pro MEC
- jednotky vykonu GFLOP (tzn jedno cislo INT) pro GPU a pro CPU + Mnozstvi RAM v GB
- propojit se subtype MEC kontejneru

Auticko musi byt schopno k requesty pripojit "aditional data" coz je string (json) ktery algoritmus vyhodnoti, budou obsahovat informace o narocnisti tasku


Pokud BridgeDP nedosahne na MGWDP tak logy bridge tvrdi ze DP connected, pritom neni.