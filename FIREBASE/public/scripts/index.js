import { initializeApp } from "https://www.gstatic.com/firebasejs/9.22.2/firebase-app.js";
import { getDatabase, ref, onValue } from "https://www.gstatic.com/firebasejs/9.22.2/firebase-database.js";

const firebaseConfig = {
  apiKey: "TU_API_KEY",
  authDomain: "TU_DOMINIO.firebaseapp.com",
  databaseURL: "https://capstone-b36f2-default-rtdb.firebaseio.com/",
  projectId: "capstone-b36f2",
  storageBucket: "capstone-b36f2.appspot.com",
  messagingSenderId: "XXXXXXX",
  appId: "XXXXXXX"
};

const app = initializeApp(firebaseConfig);
const db = getDatabase(app);
const agentsRef = ref(db, 'Agents');

const agentContainer = document.getElementById('content-sign-in');
const agentElements = {};
const lastSeen = {};

function createAgentCard(id) {
  const div = document.createElement("div");
  div.className = "card agent-card";
  div.id = `agent-${id}`;
  div.innerHTML = `
    <p><strong>Agente ${id}</strong></p>
    <p>SOC: <span id="soc-${id}"></span> %</p>
    <p>Barra: <span id="barra-${id}"></span> V</p>
    <p>Corriente: <span id="iout-${id}"></span> A</p>
    <p>Celda 1: <span id="v1-${id}"></span> V</p>
    <p>Celda 2: <span id="v2-${id}"></span> V</p>
    <p>Celda 3: <span id="v3-${id}"></span> V</p>
    <p>Celda 4: <span id="v4-${id}"></span> V</p>
  `;
  agentContainer.appendChild(div);
  agentElements[id] = div;
}

onValue(agentsRef, (snapshot) => {
  const data = snapshot.val();
  if (!data) return;

  const now = Date.now();

  Object.keys(data).forEach(id => {
    const agent = data[id];
    lastSeen[id] = now;

    if (!agentElements[id]) createAgentCard(id);

    document.getElementById(`soc-${id}`).innerText = agent.soc.toFixed(2);
    document.getElementById(`barra-${id}`).innerText = agent.barra.toFixed(2);
    document.getElementById(`iout-${id}`).innerText = agent.iout.toFixed(2);
    document.getElementById(`v1-${id}`).innerText = agent.celda_1.toFixed(2);
    document.getElementById(`v2-${id}`).innerText = agent.celda_2.toFixed(2);
    document.getElementById(`v3-${id}`).innerText = agent.celda_3.toFixed(2);
    document.getElementById(`v4-${id}`).innerText = agent.celda_4.toFixed(2);

    agentElements[id].style.display = "block";
  });
});

// Verifica inactividad cada segundo
setInterval(() => {
  const now = Date.now();
  Object.keys(lastSeen).forEach(id => {
    if (now - lastSeen[id] < 1000) {
      if (agentElements[id]) {
        agentElements[id].style.display = "none";
      }
    }
  });
}, 1000);
