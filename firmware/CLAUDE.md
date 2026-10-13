# GRain — firmware

Firmware ESP32 (PlatformIO, framework `arduino`, placa `esp32dev`).

## Antes de qualquer tarefa
Leia `docs/status.md` (onde paramos) e `docs/hardware.md` (pinos e periféricos).

## Ao terminar uma tarefa
- Atualize `docs/status.md` (estado, próximos passos, problemas).
- Mudou fiação ou pino → `docs/hardware.md` **e** `include/config.h`.

## Ao concluir uma fase
Atualize toda a documentação do repositório antes de seguir:
- `../README.md` (tabela "Situação atual" e seções afetadas: hardware, bibliotecas, o que esperar, estrutura).
- `../docs/PLANO.md` e `../docs/plano.html` (status da fase e resultado); republicar o artefato do plano.
- `docs/status.md` e `docs/hardware.md`.
- Conferir que não há dados pessoais, commit e push.

## Comandos
```bash
pio run -e dev                # compilar
pio run -e dev -t upload      # gravar via USB
pio device monitor            # log serial (com decoder de backtrace)
pio run -e release            # build de release
```

## Convenções
- `src/main.cpp` enxuto; separe a lógica em módulos em `lib/` quando crescer.
- Logs com `log_i/log_w/log_e` (Arduino) ou `ESP_LOGx` (ESP-IDF), com uma `TAG` por módulo.
- Repositório público: dados do local (altitude, andar) só em `include/site.h` (fora do git); nada pessoal em docs ou commits.
- Segredos fora do git. Versões de bibliotecas fixadas em `lib_deps`.
- Só dizer "pronto" com `dev` e `release` compilando. Relatar falhas como são.
- Mudou hardware → upload + monitor serial; o usuário descreve o que vê na placa.
