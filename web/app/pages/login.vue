<script setup lang="ts">
useHead({ title: 'Entrar · GWethr' })

const password = ref('')
const error = ref('')
const busy = ref(false)
const { fetch: refreshSession } = useUserSession()

async function submit() {
  error.value = ''
  busy.value = true
  try {
    await $fetch('/api/auth/login', { method: 'POST', body: { password: password.value } })
    await refreshSession()
    await navigateTo('/')
  } catch (e: any) {
    error.value = e?.statusCode === 401 ? 'Senha incorreta. Tente de novo.' : 'Não foi possível entrar agora.'
    password.value = ''
  } finally {
    busy.value = false
  }
}
</script>

<template>
  <main class="login">
    <form class="card" @submit.prevent="submit">
      <span class="eyebrow">Estação meteorológica</span>
      <h1>GWethr</h1>
      <label for="password">Senha do painel</label>
      <input
        id="password"
        v-model="password"
        type="password"
        autocomplete="current-password"
        required
        autofocus
      >
      <p v-if="error" class="error" role="alert">{{ error }}</p>
      <button class="btn primary" type="submit" :disabled="busy">{{ busy ? 'Entrando…' : 'Entrar' }}</button>
    </form>
  </main>
</template>

<style scoped>
.login {
  min-height: 100dvh;
  display: grid;
  place-items: center;
  padding-inline: 16px;
}
.card {
  width: min(360px, 100%);
  background: var(--surface);
  border: 1px solid var(--line);
  border-radius: 12px;
  padding: 24px;
  display: grid;
  gap: 10px;
}
h1 { font-size: 2rem; font-weight: 800; font-stretch: 118%; margin-bottom: 8px; }
label { font-size: 0.85rem; color: var(--muted); }
input {
  font: inherit;
  padding: 10px 12px;
  border: 1px solid var(--line);
  border-radius: 8px;
  background: var(--bg);
  color: var(--fg);
}
.error { color: var(--crit); margin: 0; font-size: 0.9rem; }
.btn { margin-top: 6px; padding: 10px; }
</style>
