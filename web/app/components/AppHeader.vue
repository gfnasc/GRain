<script setup lang="ts">
// Cabeçalho comum: nome, navegação, estado da estação e sair.
const props = defineProps<{ lastSeenAt?: string | null }>()
const { clear } = useUserSession()

const now = ref(Date.now())
let timer: ReturnType<typeof setInterval> | undefined
onMounted(() => (timer = setInterval(() => (now.value = Date.now()), 30_000)))
onBeforeUnmount(() => clearInterval(timer))

const online = computed(() => !!props.lastSeenAt && now.value - new Date(props.lastSeenAt).getTime() < 15 * 60_000)
const ago = computed(() => {
  if (!props.lastSeenAt) return 'nunca'
  const min = Math.round((now.value - new Date(props.lastSeenAt).getTime()) / 60000)
  if (min < 1) return 'agora há pouco'
  if (min < 60) return `há ${min} min`
  const h = Math.floor(min / 60)
  return h < 48 ? `há ${h} h` : `há ${Math.floor(h / 24)} dias`
})

async function logout() {
  await clear()
  await navigateTo('/login')
}
</script>

<template>
  <header class="top">
    <div class="brand">
      <span class="eyebrow">Estação meteorológica</span>
      <h1>GWeather</h1>
    </div>
    <nav aria-label="Seções">
      <NuxtLink to="/">Agora</NuxtLink>
      <NuxtLink to="/historico">Histórico</NuxtLink>
      <NuxtLink to="/display">Display</NuxtLink>
    </nav>
    <div class="actions">
      <span v-if="lastSeenAt !== undefined" class="pill" :class="online ? 'ok' : 'off'">
        {{ online ? 'Online' : 'Sem notícias' }} · {{ ago }}
      </span>
      <button class="btn" type="button" @click="logout">Sair</button>
    </div>
  </header>
</template>

<style scoped>
.top {
  display: flex;
  flex-wrap: wrap;
  align-items: end;
  gap: 12px 24px;
  padding-bottom: 16px;
  border-bottom: 2px solid var(--fg);
}
.brand { margin-right: auto; }
h1 { font-size: clamp(2rem, 5vw, 2.8rem); font-weight: 800; font-stretch: 118%; }
nav { display: flex; gap: 4px; }
nav a {
  color: var(--fg);
  text-decoration: none;
  font-weight: 650;
  padding: 6px 12px;
  border-radius: 8px;
}
nav a:hover { background: var(--sunk); }
nav a.router-link-exact-active { background: var(--fg); color: var(--bg); }
nav a:focus-visible { outline: 2px solid var(--accent); outline-offset: 2px; }
.actions { display: flex; gap: 10px; align-items: center; flex-wrap: wrap; }
.pill { font-family: var(--ff-mono); font-size: 0.78rem; padding: 4px 10px; border-radius: 999px; border: 1px solid var(--line); }
.pill.ok { color: var(--ok); border-color: var(--ok); }
.pill.off { color: var(--warn); border-color: var(--warn); }
@media (max-width: 560px) {
  .brand { flex-basis: 100%; }
}
</style>
