#pragma once

// API local /api/v1 e página mínima em "/" (porta 80).
// O servidor só roda com Wi-Fi conectado e a rede de configuração fechada,
// porque enquanto ela está aberta a porta 80 pertence ao WiFiManager.
// Contrato: docs/api-v1.md (na raiz do repositório).
void webapiLoop();
