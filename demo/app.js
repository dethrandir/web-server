const button = document.getElementById("hello");
const message = document.getElementById("message");

let clicks = 0;

button.addEventListener("click", () => {
  clicks += 1;
  message.textContent = `Merhaba! ${clicks} kez tıkladın.`;
});
