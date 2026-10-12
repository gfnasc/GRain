// https://nuxt.com/docs/api/configuration/nuxt-config
export default defineNuxtConfig({
  modules: ['nuxt-auth-utils'],
  compatibilityDate: '2025-07-15',
  devtools: { enabled: true },

  css: ['~/assets/main.css'],

  app: {
    head: {
      htmlAttrs: { lang: 'pt-BR' },
      title: 'GWeather',
      meta: [{ name: 'viewport', content: 'width=device-width, initial-scale=1' }],
      link: [
        { rel: 'preconnect', href: 'https://fonts.googleapis.com' },
        { rel: 'preconnect', href: 'https://fonts.gstatic.com', crossorigin: '' },
        {
          rel: 'stylesheet',
          href: 'https://fonts.googleapis.com/css2?family=Archivo:wdth,wght@100..125,400..800&family=JetBrains+Mono:wght@400;600&display=swap',
        },
      ],
    },
  },

  runtimeConfig: {
    // Hash (scrypt) da senha do painel. Definido por NUXT_PANEL_PASSWORD_HASH.
    panelPasswordHash: '',
    // A sessão (cookie criptografado, 30 dias) usa NUXT_SESSION_PASSWORD (nuxt-auth-utils).
    public: {
      // Fuso usado para exibir horários e separar os dias (NUXT_PUBLIC_TZ).
      tz: 'UTC',
    },
  },
})
