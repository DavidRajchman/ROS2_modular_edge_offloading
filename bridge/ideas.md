Use modified transport libs

After receiving offloading allowed message for certain outputs, create a maping which will forward messages from certain vehicle topics, to the MEC server. Then it will setup a listener for the output topic from MEC and forward it to the vehicle

The response will be a json which the bridge will intercept and setup the forwarding accrodingly, it will also notify the VHC and MEC server to enable the handlers

{
  "RequestID": 0,
  "GrantType": 0,
  "MECserverLOC":"",
  "InputsForProcessing": [],
  "ProcessedOutputs": [],
}


request info
task for computes
priority (QoS)
time to complete


popis funkcionality:

VHC pošle request pro offloading, pro určitý typ výpočtu. OM zjistí zda má dostatečnou serverovou kapacitu, pokud ano tak spustí nový kontejner na MEC a pošle info do Bridge aby sestavil trasu. Bridge sestaví cestu a pošle info do VHC který zapne handlery. 

Pro určité stupně QoS bude designovaný jeden topic signalizující start výpočtu. V tu chvíly začne VHC výpočet jelikož nevý zda bude možný oflloading stihnut. Startovací topic se zároveň přepošle do OM, který rozhodne zda je schopen výpočet stihnout doručit do deadlinu. Pokud ano pošle potvrzení přes Bridge do VHC, který ukončí lokální výpočet a začne čekat na externí výpočet 

requesty pro offloading se neposílají s každým výpočtem ale periodicky (např 10s). První request vyžaduje nějaký čas na spuštění nového kontejneru a přiřazení cesty bridgem, následující requesty přijdu dříve než deadline uplyne a tím pádem již sestavená offloading trasa bude ponechána. Pokud nový request nepřijde než uplyne deadline, bude trasa zničena a kontejner vypnut. Trasu je také možné zničit na žádost VHC (např konec jízdy)

Navíc, v Bridge bude FIFO fronta která bude cashovat veškeré zprávy topiců potřebných pro výpočet. Tato fronta se bude vyprazdňovat pokaždé když dojde k dokončení předchozího výpočtu. V případě že by něco selhalo, může být tato fronta použita k znovuspuštění výpočtu v jiném kontejneru