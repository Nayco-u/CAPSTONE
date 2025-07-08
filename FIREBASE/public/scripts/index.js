import { initializeApp } from "https://www.gstatic.com/firebasejs/9.22.2/firebase-app.js";
import { getDatabase, ref, onValue } from "https://www.gstatic.com/firebasejs/9.22.2/firebase-database.js";

const firebaseConfig = {
  apiKey: "AIzaSyAVt_Gn_2jqQYufbcg4GzsJ6Q6MoozBGYU",
  authDomain: "https://capstone-b36f2.firebaseapp.com/",
  databaseURL: "https://capstone-b36f2-default-rtdb.firebaseio.com/",
  projectId: "capstone-b36f2",
  storageBucket: "capstone-b36f2.appspot.com",
  messagingSenderId: "XXXXXXX",
  appId: "XXXXXXX"
};

// Inicializar Firebase
console.log("Inicializando Firebase...");
const app = initializeApp(firebaseConfig);
const db = getDatabase(app);
const agentsRef = ref(db, "/");


// Referencia al contenedor dinámico
const agentContainer = document.getElementById('content-sign-in');
if (!agentContainer) {
  console.error("No se encontró el contenedor dinámico con ID 'content-sign-in'.");
}

// Cargar Chart.js dinámicamente si no está incluido en el HTML
if (typeof Chart === "undefined") {
  const script = document.createElement('script');
  script.src = "https://cdn.jsdelivr.net/npm/chart.js";
  script.onload = () => { window.ChartLoaded = true; };
  document.head.appendChild(script);
}

// --- Configuración de buffers para los últimos 60 datos ---
const MAX_POINTS = 60;
const agentIds = ["201", "202", "203"];
const barraData = {};
const celda1Data = {};
const timeData = {}; // Nuevo: buffer de tiempo por agente
const lastTime = {}; // Nuevo: último tiempo registrado por agente
const socData = {};
const iBatteryData = {};
const iBarraData = {};
agentIds.forEach(id => {
  barraData[id] = [];
  celda1Data[id] = [];
  timeData[id] = [];
  lastTime[id] = null;
  socData[id] = [];
  iBatteryData[id] = [];
  iBarraData[id] = [];
});

// --- Inicialización de gráficos ---
let chartBarra, chartCelda1, chartSOC, chartIBattery, chartIBarra;
function initCharts() {
  const ctxBarra = document.getElementById('chart-barra').getContext('2d');
  const ctxCelda1 = document.getElementById('chart-celda1').getContext('2d');
  const ctxSOC = document.getElementById('chart-soc').getContext('2d');
  const ctxIBattery = document.getElementById('chart-i_battery').getContext('2d');
  const ctxIBarra = document.getElementById('chart-i_barra').getContext('2d');
  const colors = ['#ff6384', '#36a2eb', '#4bc0c0'];

  chartBarra = new Chart(ctxBarra, {
    type: 'line',
    data: {
      labels: Array(MAX_POINTS).fill(''),
      datasets: agentIds.map((id, idx) => ({
        label: id,
        data: [],
        borderColor: colors[idx],
        backgroundColor: colors[idx] + '33',
        fill: false,
        tension: 0.2,
        spanGaps: true
      }))
    },
    options: {
      animation: false,
      responsive: false,
      plugins: { legend: { display: true } },
      scales: {
        y: { title: { display: true, text: 'Voltaje (V)' } },
        x: { title: { display: true, text: 'Tiempo (s)' } }
      }
    }
  });

  chartCelda1 = new Chart(ctxCelda1, {
    type: 'line',
    data: {
      labels: Array(MAX_POINTS).fill(''),
      datasets: agentIds.map((id, idx) => ({
        label: id,
        data: [],
        borderColor: colors[idx],
        backgroundColor: colors[idx] + '33',
        fill: false,
        tension: 0.2,
        spanGaps: true
      }))
    },
    options: {
      animation: false,
      responsive: false,
      plugins: { legend: { display: true } },
      scales: {
        y: { title: { display: true, text: 'Voltaje (V)' } },
        x: { title: { display: true, text: 'Tiempo (s)' } }
      }
    }
  });

  chartSOC = new Chart(ctxSOC, {
    type: 'line',
    data: {
      labels: Array(MAX_POINTS).fill(''),
      datasets: agentIds.map((id, idx) => ({
        label: id,
        data: [],
        borderColor: colors[idx],
        backgroundColor: colors[idx] + '33',
        fill: false,
        tension: 0.2,
        spanGaps: true
      }))
    },
    options: {
      animation: false,
      responsive: false,
      plugins: { legend: { display: true } },
      scales: {
        y: { title: { display: true, text: 'SOC (%)' } },
        x: { title: { display: true, text: 'Tiempo (s)' } }
      }
    }
  });

  chartIBattery = new Chart(ctxIBattery, {
    type: 'line',
    data: {
      labels: Array(MAX_POINTS).fill(''),
      datasets: agentIds.map((id, idx) => ({
        label: id,
        data: [],
        borderColor: colors[idx],
        backgroundColor: colors[idx] + '33',
        fill: false,
        tension: 0.2,
        spanGaps: true
      }))
    },
    options: {
      animation: false,
      responsive: false,
      plugins: { legend: { display: true } },
      scales: {
        y: { title: { display: true, text: 'Corriente (A)' } },
        x: { title: { display: true, text: 'Tiempo (s)' } }
      }
    }
  });

  chartIBarra = new Chart(ctxIBarra, {
    type: 'line',
    data: {
      labels: Array(MAX_POINTS).fill(''),
      datasets: agentIds.map((id, idx) => ({
        label: id,
        data: [],
        borderColor: colors[idx],
        backgroundColor: colors[idx] + '33',
        fill: false,
        tension: 0.2,
        spanGaps: true
      }))
    },
    options: {
      animation: false,
      responsive: false,
      plugins: { legend: { display: true } },
      scales: {
        y: { title: { display: true, text: 'Corriente (A)' } },
        x: { title: { display: true, text: 'Tiempo (s)' } }
      }
    }
  });
}

// Esperar a que Chart.js esté cargado antes de inicializar los gráficos
function waitForChartJsAndInit() {
  if (typeof Chart !== "undefined") {
    initCharts();
  } else {
    setTimeout(waitForChartJsAndInit, 100);
  }
}
waitForChartJsAndInit();

// Escuchar cambios en la base de datos
onValue(agentsRef, (snapshot) => {
  console.log("Datos recibidos de Firebase");
  const data = snapshot.val();
  if (!data) {
    console.warn("No se encontraron datos en la base de datos.");
    return;
  }

  let shouldUpdate = false;
  const agentes = [201, 202, 203];

  agentes.forEach((id, idx) => {
    const v_barra   = (data[`${id}_v_barra`]   ?? 0) * 0.0001875 * 2;
    const v_celda1  = (data[`${id}_v_celda1`]  ?? 0) * 0.0001875;
    const i_battery = (data[`${id}_i_battery`] ?? 0) * 0.0001875 / 0.103;
    const i_barra   = (data[`${id}_i_barra`]   ?? 0) * 0.0001875 / 0.103;
    const soc       = (data[`${id}_soc`]       ?? 0) / 100;
    const time      =  data[`${id}_time`]      ?? 0;

    // Actualizar HTML
    document.getElementById(`v_barra-${id}`).innerText = v_barra.toFixed(2);
    document.getElementById(`v_celda1-${id}`).innerText = v_celda1.toFixed(2);
    document.getElementById(`i_battery-${id}`).innerText = i_battery.toFixed(2);
    document.getElementById(`i_barra-${id}`).innerText = i_barra.toFixed(2);
    document.getElementById(`soc-${id}`).innerText = soc.toFixed(2);

    // Actualizar buffers si cambió el tiempo
    if (typeof time !== "undefined") {
      if (lastTime[id] !== time) {
        shouldUpdate = true;
        lastTime[id] = time;

        if (barraData[id]) {
          barraData[id].push(v_barra);
          if (barraData[id].length > MAX_POINTS) barraData[id].shift();
        }
        if (celda1Data[id]) {
          celda1Data[id].push(v_celda1);
          if (celda1Data[id].length > MAX_POINTS) celda1Data[id].shift();
        }
        if (socData[id]) {
          socData[id].push(soc);
          if (socData[id].length > MAX_POINTS) socData[id].shift();
        }
        if (iBatteryData[id]) {
          iBatteryData[id].push(i_battery);
          if (iBatteryData[id].length > MAX_POINTS) iBatteryData[id].shift();
        }
        if (iBarraData[id]) {
          iBarraData[id].push(i_barra);
          if (iBarraData[id].length > MAX_POINTS) iBarraData[id].shift();
        }
        if (timeData[id]) {
          timeData[id].push(time);
          if (timeData[id].length > MAX_POINTS) timeData[id].shift();
        }
      }
    }
  });

  // Actualizar gráficos
  if (shouldUpdate && chartBarra && chartCelda1 && chartSOC && chartIBattery && chartIBarra) {
    let mainTime = [];
    agentes.forEach(id => {
      if (timeData[id].length > mainTime.length) mainTime = timeData[id];
    });

    chartBarra.data.labels = mainTime;
    chartCelda1.data.labels = mainTime;
    chartSOC.data.labels = mainTime;
    chartIBattery.data.labels = mainTime;
    chartIBarra.data.labels = mainTime;

    agentes.forEach((id, idx) => {
      chartBarra.data.datasets[idx].data = barraData[id];
      chartCelda1.data.datasets[idx].data = celda1Data[id];
      chartSOC.data.datasets[idx].data = socData[id];
      chartIBattery.data.datasets[idx].data = iBatteryData[id];
      chartIBarra.data.datasets[idx].data = iBarraData[id];
    });

    chartBarra.update('none');
    chartCelda1.update('none');
    chartSOC.update('none');
    chartIBattery.update('none');
    chartIBarra.update('none');
  }
});