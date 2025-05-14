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
const agentsRef = ref(db, 'Agents');

// Referencia al contenedor dinámico
const agentContainer = document.getElementById('content-sign-in');
if (!agentContainer) {
  console.error("No se encontró el contenedor dinámico con ID 'content-sign-in'.");
}

// Función para crear tarjetas de agentes
function createAgentCard(id) {
  console.log(`Creando tarjeta para el agente ${id}`);
  const div = document.createElement("div");
  div.className = "card agent-card";
  div.id = `agent-${id}`;
  div.innerHTML = `
    <p><strong>Agente ${id}</strong></p>
    <p>Barra: <span id="barra-${id}"></span> V</p>
    <p>Celda 1: <span id="celda1-${id}"></span> V</p>
  `;
  agentContainer.appendChild(div);
}

// Escuchar cambios en la base de datos
onValue(agentsRef, (snapshot) => {
  console.log("Datos recibidos de Firebase");
  const data = snapshot.val();
  if (!data) {
    console.warn("No se encontraron datos en la base de datos.");
    return;
  }

  Object.keys(data).forEach(id => {
    const agent = data[id];
    if (!document.getElementById(`agent-${id}`)) {
      createAgentCard(id);
    }

    document.getElementById(`barra-${id}`).innerText = agent.barra.toFixed(2);
    document.getElementById(`celda1-${id}`).innerText = agent.celda1.toFixed(2);
  });
});