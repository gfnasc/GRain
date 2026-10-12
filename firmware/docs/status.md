# Status

Memória de continuidade entre sessões. O Claude lê isto ao começar e **atualiza ao terminar** cada tarefa.
Plano geral: `../../docs/PLANO.md` e `../../docs/plano.html`.

## Arquitetura (v4)
Painel em Nuxt na Vercel (pasta `web/`, fase 6). O ESP32 mede, mostra no OLED, agenda mensagens,
envia os dados para a nuvem e mantém uma API local + página mínima em `gweather.local`.

## Estado atual
- Fases 1–3 concluídas (montagem, sensores, OLED com abertura e páginas).
- Fase 4 concluída e validada na placa:
  - `src/netmgr.*`: WiFiManager não bloqueante; rede "GWeather-Setup" com senha aleatória
    (gerada após ligar o rádio, guardada na NVS em `gweather/portal_pass`) e mostrada no OLED.
  - NTP com fuso de `/config.json` (`time.tz`); mDNS `gweather.local` (ping ok a partir do Mac).
  - `src/settings.*`: `/config.json` em LittleFS, criado com valores de `config.h`/`site.h`.
  - `src/console.*`: `ajuda`, `status`, `config`, `set`, `config padrao`, `wifi portal|esquecer`, `reiniciar`.
  - OLED: relógio no cabeçalho, página Rede, tela fixa "Configurar Wi-Fi" com o portal aberto.
  - Serial: uma linha por leitura.
  - `partitions.csv`: app0/app1 de 1,44 MB (OTA) + LittleFS 1 MB + coredump.
  - Teste: após vários reinícios, voltou sozinho ao Wi-Fi, hora sincronizada em ~2 s, config preservada.
- `dev` e `release` compilam (release ~70 % de uma partição de app).

## Em andamento
- Fase 5: API local `/api/v1` (WebServer do core) + página mínima embutida no firmware.

## Próximos passos
1. Fase 5: `src/webapi.*`, `docs/api-v1.md`, página mínima; senha nas rotas que alteram algo.
2. Fase 6: app Nuxt em `web/` na Vercel, Postgres, ingestão e envio pelo ESP32 com reserva offline.

## Problemas conhecidos
- Log `[W][Wire.cpp] Bus already started in Master Mode` no boot: as bibliotecas chamam `Wire.begin()` de novo. Inofensivo.
- Abrir a porta serial no macOS reinicia a placa (DTR/RTS).
- Durante o portal do WiFiManager a porta 80 é dele; o servidor da API só pode subir com o portal fechado.
- Altitude do terreno em `site.h` é estimada; conferir com a altitude da rua e com o QNH do METAR do aeroporto mais próximo.
