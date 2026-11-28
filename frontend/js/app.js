"use strict";

const state = {
  customers: [],
  rooms: [],
  reservations: [],
  payments: [],
  waitingEntries: [],
  smartRecommendations: [],
  dashboard: null,
  roomFilter: "all",
  reservationFilter: "all"
};

const ROOM_CATEGORY_IMAGES = {
  Single: "/assets/room-single.jpg",
  Double: "/assets/room-double.jpg",
  Deluxe: "/assets/room-deluxe.jpg"
};

const $ = (selector, root = document) => root.querySelector(selector);
const $$ = (selector, root = document) => Array.from(root.querySelectorAll(selector));

function escapeHtml(value) {
  return String(value ?? "").replace(/[&<>"']/g, (character) => ({
    "&": "&amp;",
    "<": "&lt;",
    ">": "&gt;",
    '"': "&quot;",
    "'": "&#39;"
  })[character]);
}

function updateConnectionStatus(isConnected) {
  const statusText = $("#systemStatusText");
  const statusPulse = $("#serverStatusPulse");
  const liveText = $("#liveStatusText");
  const livePulse = $("#liveStatusPulse");
  statusText.textContent = isConnected ? "System connected" : "Connection unavailable";
  statusPulse.style.background = isConnected ? "#57c894" : "#d65b64";
  liveText.textContent = isConnected ? "LIVE DATA" : "OFFLINE";
  livePulse.style.background = isConnected ? "#57c894" : "#d65b64";
}

function money(value) {
  const amount = Number(value);
  if (!Number.isFinite(amount)) return "—";
  return `Rs. ${amount.toLocaleString("en-PK", {
    minimumFractionDigits: 2,
    maximumFractionDigits: 2
  })}`;
}

function dateLabel(value) {
  if (!value) return "—";
  const date = new Date(`${value}T00:00:00`);
  return Number.isNaN(date.getTime())
    ? escapeHtml(value)
    : date.toLocaleDateString(undefined, {
        day: "2-digit",
        month: "short",
        year: "numeric"
      });
}

function statusClass(value) {
  return `status-${String(value).toLowerCase().replace(/[^a-z0-9]+/g, "")}`;
}

async function api(path, options = {}) {
  const response = await fetch(path, {
    method: options.method || "GET",
    headers: options.body
      ? { "Content-Type": "application/json" }
      : undefined,
    body: options.body ? JSON.stringify(options.body) : undefined
  });
  let payload = null;
  try {
    payload = await response.json();
  } catch {
    throw new Error(`The server returned an unreadable response (${response.status}).`);
  }
  if (!response.ok) {
    throw new Error(payload?.error || `Request failed (${response.status}).`);
  }
  return payload;
}

function showToast(message, isError = false) {
  const toast = document.createElement("div");
  toast.className = `toast${isError ? " error" : ""}`;
  toast.textContent = message;
  $("#toastRegion").append(toast);
  window.setTimeout(() => toast.remove(), 4200);
}

function setFormMessage(elementId, message, isError = false) {
  const element = document.getElementById(elementId);
  if (!element) return;
  element.textContent = message;
  element.classList.toggle("error", isError);
  element.classList.toggle("success", Boolean(message) && !isError);
}

function customerById(customerId) {
  return state.customers.find((customer) => customer.customer_id === customerId);
}

function renderReservationStatusChart(reservations) {
  const statuses = [
    ["Confirmed", reservations.confirmed],
    ["Checked in", reservations.checked_in],
    ["Checked out", reservations.checked_out],
    ["Cancelled", reservations.cancelled]
  ];
  const maximum = Math.max(0, ...statuses.map(([, count]) => Number(count) || 0));
  $("#reservationStatusChart").innerHTML = statuses.map(([label, count]) => {
    const value = Number(count) || 0;
    const width = maximum === 0 ? 0 : (value / maximum) * 100;
    return `<div class="reservation-status-item">
      <div class="reservation-status-label"><span>${label}</span><strong>${value}</strong></div>
      <div class="reservation-bar-track">
        <span class="reservation-bar-fill" style="width:${width.toFixed(1)}%" title="${label}: ${value} reservations"></span>
      </div>
    </div>`;
  }).join("");
}

function renderDashboard() {
  const dashboard = state.dashboard;
  if (!dashboard) return;
  const rooms = dashboard.rooms;
  const reservations = dashboard.reservations;
  $("#statRooms").textContent = rooms.total;
  $("#statRoomsAvailable").textContent = `${rooms.available} available`;
  $("#statRoomsOccupied").textContent = `${rooms.occupied} occupied`;
  $("#statCustomers").textContent = dashboard.customers;
  $("#statReservations").textContent = reservations.total;
  $("#statReservationsConfirmed").textContent =
    `${reservations.confirmed} confirmed`;
  $("#statReservationsCheckedIn").textContent =
    `${reservations.checked_in} checked in`;
  $("#statPayments").textContent = money(dashboard.payments.total_amount);
  $("#statPaymentCount").textContent =
    `${dashboard.payments.count} payments recorded`;

  const percent = rooms.occupancy_rate || 0;
  $("#occupancyRate").textContent = `${percent.toFixed(1)}% occupied`;
  $("#occupancyPercent").textContent = `${Math.round(percent)}%`;
  $("#occupancyDonut").style.setProperty(
    "--occupied",
    `${(Math.min(100, Math.max(0, percent)) * 3.6).toFixed(2)}deg`
  );
  $("#occupiedCount").textContent = rooms.occupied;
  $("#availableCount").textContent = rooms.available;
  $("#maintenanceCount").textContent = rooms.maintenance;
  $("#occupancyTotal").textContent = rooms.total;
  $("#reservationStatusSummary").textContent =
    `${reservations.confirmed} confirmed · ${reservations.checked_in} checked in · ${reservations.checked_out} checked out`;
  renderReservationStatusChart(reservations);

  const recent = dashboard.recent_reservations || [];
  $("#recentReservations").innerHTML = recent.length
    ? recent.map((reservation) => {
        const customer = customerById(reservation.customer_id);
        return `<tr>
          <td class="id-cell">#${escapeHtml(reservation.reservation_id)}</td>
          <td class="primary-cell">${escapeHtml(customer?.full_name || `Customer ${reservation.customer_id}`)}</td>
          <td>Room ${escapeHtml(reservation.room_number)}</td>
          <td>${dateLabel(reservation.check_in_date)} – ${dateLabel(reservation.check_out_date)}</td>
          <td class="primary-cell">${money(reservation.total_cost)}</td>
          <td><span class="status-badge ${statusClass(reservation.status)}">${escapeHtml(reservation.status)}</span></td>
        </tr>`;
      }).join("")
    : '<tr><td colspan="6" class="table-message">No reservations are stored yet.</td></tr>';
}

function renderCustomers() {
  const filter = ($("#customerFilter").value || "").trim().toLowerCase();
  const customers = state.customers.filter((customer) =>
    [
      customer.customer_id,
      customer.full_name,
      customer.phone_number,
      customer.email,
      customer.identity_document
    ].some((value) => String(value).toLowerCase().includes(filter))
  );
  $("#customerCount").textContent = state.customers.length;
  $("#customerSummaryTotal").textContent = state.customers.length;
  $("#customersTable").innerHTML = customers.length
    ? customers.map((customer) => `<tr>
        <td class="id-cell">${escapeHtml(customer.customer_id)}</td>
        <td class="primary-cell">${escapeHtml(customer.full_name)}</td>
        <td>${escapeHtml(customer.phone_number)}</td>
        <td>${escapeHtml(customer.email)}</td>
        <td>${escapeHtml(customer.identity_document)}</td>
        <td><div class="row-actions">
          <button class="table-action" data-edit-customer="${escapeHtml(customer.customer_id)}">Edit</button>
          <button class="table-action danger" data-delete-customer="${escapeHtml(customer.customer_id)}">Delete</button>
        </div></td>
      </tr>`).join("")
    : `<tr><td colspan="6" class="table-message">${filter ? "No customers match this filter." : "No customers are stored yet."}</td></tr>`;
}

function renderCustomerSearchResult(customer) {
  $("#customerSearchResult").classList.remove("empty-state");
  $("#customerSearchResult").innerHTML = `<div class="customer-result-grid">
    <div class="result-field"><span>Customer ID</span><strong>${escapeHtml(customer.customer_id)}</strong></div>
    <div class="result-field"><span>Full name</span><strong>${escapeHtml(customer.full_name)}</strong></div>
    <div class="result-field"><span>Phone</span><strong>${escapeHtml(customer.phone_number)}</strong></div>
    <div class="result-field"><span>Email</span><strong>${escapeHtml(customer.email)}</strong></div>
    <div class="result-field"><span>Identity document</span><strong>${escapeHtml(customer.identity_document)}</strong></div>
  </div>`;
}

function renderSmartRoomRecommendations() {
  $("#smartRoomCount").textContent = state.smartRecommendations.length;
  if (!state.smartRecommendations.length) {
    $("#smartRoomResults").innerHTML =
      '<div class="smart-room-empty">No suitable rooms are available for these requirements.</div>';
    return;
  }

  $("#smartRoomResults").innerHTML =
    state.smartRecommendations.map((recommendation) => {
      const category = String(recommendation.category);
      const image =
        ROOM_CATEGORY_IMAGES[category] || ROOM_CATEGORY_IMAGES.Double;
      return `<article class="smart-room-card">
        <img src="${image}" alt="${escapeHtml(category)} room category" loading="lazy">
        <div class="smart-room-copy">
          <div class="smart-room-heading">
            <h3>Room ${escapeHtml(recommendation.room_number)}</h3>
            <span class="status-badge ${statusClass(recommendation.status)}">${escapeHtml(recommendation.status)}</span>
          </div>
          <p class="room-card-category">${escapeHtml(category)} accommodation</p>
          <p class="room-card-price">${money(recommendation.price_per_night)} <small>per night</small></p>
          <p class="smart-room-capacity">Fits up to ${escapeHtml(recommendation.maximum_guests)} ${recommendation.maximum_guests === 1 ? "guest" : "guests"}</p>
          <p class="smart-room-total"><strong>${money(recommendation.total_cost)}</strong> for ${escapeHtml(recommendation.number_of_nights)} ${recommendation.number_of_nights === 1 ? "night" : "nights"}</p>
          <p class="smart-room-reason">${escapeHtml(recommendation.reason)}</p>
          <button class="button button-primary smart-room-book" type="button" data-smart-room="${escapeHtml(recommendation.room_number)}">
            Use this room
          </button>
        </div>
      </article>`;
    }).join("");
}

function renderRooms() {
  const rooms = state.roomFilter === "all"
    ? state.rooms
    : state.rooms.filter((room) => room.status === state.roomFilter);
  $("#roomCount").textContent = state.rooms.length;
  $("#roomsGrid").innerHTML = rooms.length
    ? rooms.map((room) => {
        const category = String(room.category);
        const image = ROOM_CATEGORY_IMAGES[category] || ROOM_CATEGORY_IMAGES.Double;
        return `<article class="room-card">
          <div class="room-card-image-wrap">
            <img class="room-card-image" src="${image}" alt="Illustrative ${escapeHtml(category)} room category photograph" loading="lazy">
            <span class="room-image-caption">Category inspiration</span>
          </div>
          <div class="room-card-body">
            <div class="room-card-topline">
              <div><h3 class="room-card-number">Room ${escapeHtml(room.room_number)}</h3><p class="room-card-category">${escapeHtml(category)} accommodation</p></div>
              <span class="status-badge ${statusClass(room.status)}">${escapeHtml(room.status)}</span>
            </div>
            <p class="room-card-price">${money(room.price_per_night)} <small>per night</small></p>
          </div>
        </article>`;
      }).join("")
    : `<div class="table-message">${state.rooms.length ? "No rooms match this status." : "No rooms are stored yet."}</div>`;
}

function renderReservations() {
  const reservations = state.reservationFilter === "all"
    ? state.reservations
    : state.reservations.filter((reservation) =>
        reservation.status === state.reservationFilter
      );
  $("#reservationCount").textContent = state.reservations.length;
  $("#reservationsTable").innerHTML = reservations.length
    ? reservations.map((reservation) => {
        const customer = customerById(reservation.customer_id);
        const actions = [];
        if (reservation.status === "Confirmed") {
          actions.push(`<button class="table-action" data-reservation-action="check-in" data-id="${escapeHtml(reservation.reservation_id)}">Check in</button>`);
          actions.push(`<button class="table-action danger" data-reservation-action="cancel" data-id="${escapeHtml(reservation.reservation_id)}">Cancel</button>`);
        } else if (reservation.status === "CheckedIn") {
          actions.push(`<button class="table-action" data-reservation-action="check-out" data-id="${escapeHtml(reservation.reservation_id)}">Check out</button>`);
        }
        return `<tr>
          <td class="id-cell">#${escapeHtml(reservation.reservation_id)}</td>
          <td class="primary-cell">${escapeHtml(customer?.full_name || `Customer ${reservation.customer_id}`)}</td>
          <td>Room ${escapeHtml(reservation.room_number)}</td>
          <td>${dateLabel(reservation.check_in_date)}<span class="secondary-cell">to ${dateLabel(reservation.check_out_date)} · ${escapeHtml(reservation.number_of_nights)} nights</span></td>
          <td class="primary-cell">${money(reservation.total_cost)}</td>
          <td><span class="status-badge ${statusClass(reservation.status)}">${escapeHtml(reservation.status)}</span></td>
          <td><div class="row-actions">${actions.join("") || '<span class="secondary-cell">No actions</span>'}</div></td>
        </tr>`;
      }).join("")
    : `<tr><td colspan="7" class="table-message">${state.reservations.length ? "No reservations match this status." : "No reservations are stored yet."}</td></tr>`;
  renderReservationChoices();
}

function renderReservationChoices() {
  const customerSelect = $("#reservationCustomer");
  const selectedCustomer = customerSelect.value;
  customerSelect.innerHTML = '<option value="">Select a customer</option>' +
    state.customers.map((customer) =>
      `<option value="${escapeHtml(customer.customer_id)}">${escapeHtml(customer.customer_id)} · ${escapeHtml(customer.full_name)}</option>`
    ).join("");
  if (state.customers.some((customer) => String(customer.customer_id) === selectedCustomer)) {
    customerSelect.value = selectedCustomer;
  }

  const roomSelect = $("#reservationRoom");
  const selectedRoom = roomSelect.value;
  const availableRooms = state.rooms.filter((room) => room.status === "Available");
  roomSelect.innerHTML = '<option value="">Select an available room</option>' +
    availableRooms.map((room) =>
      `<option value="${escapeHtml(room.room_number)}" data-rate="${escapeHtml(room.price_per_night)}">Room ${escapeHtml(room.room_number)} · ${escapeHtml(room.category)} · ${money(room.price_per_night)}</option>`
    ).join("");
  if (availableRooms.some((room) => String(room.room_number) === selectedRoom)) {
    roomSelect.value = selectedRoom;
  }
  updateReservationEstimate();

  const paymentSelect = $("#paymentReservation");
  const selectedReservation = paymentSelect.value;
  const unpaid = state.reservations.filter((reservation) => !reservation.payment);
  paymentSelect.innerHTML = '<option value="">Select an unpaid reservation</option>' +
    unpaid.map((reservation) => {
      const customer = customerById(reservation.customer_id);
      return `<option value="${escapeHtml(reservation.reservation_id)}" data-total="${escapeHtml(reservation.total_cost)}">#${escapeHtml(reservation.reservation_id)} · ${escapeHtml(customer?.full_name || `Customer ${reservation.customer_id}`)} · ${money(reservation.total_cost)}</option>`;
    }).join("");
  if (unpaid.some((reservation) =>
      String(reservation.reservation_id) === selectedReservation)) {
    paymentSelect.value = selectedReservation;
  }
}

function renderPayments() {
  $("#paymentCount").textContent = state.payments.length;
  const totalPaid = state.payments.reduce(
    (total, payment) => total + Number(payment.amount || 0),
    0
  );
  $("#paymentTotalSummary").textContent = money(totalPaid);
  $("#paymentCountSummary").textContent = state.payments.length;
  $("#unpaidReservationCount").textContent =
    state.reservations.filter((reservation) => !reservation.payment).length;
  const paymentRows = state.payments.slice().sort(
    (left, right) => right.payment_id - left.payment_id
  );
  $("#paymentsTable").innerHTML = paymentRows.length
    ? paymentRows.map((payment) => {
        const reservation = state.reservations.find(
          (item) => item.reservation_id === payment.reservation_id
        );
        const customer = reservation
          ? customerById(reservation.customer_id)
          : null;
        return `<tr>
          <td class="id-cell">#${escapeHtml(payment.payment_id)}</td>
          <td>#${escapeHtml(payment.reservation_id)}</td>
          <td class="primary-cell">${escapeHtml(customer?.full_name || "—")}</td>
          <td class="primary-cell">${money(payment.amount)}</td>
          <td>${escapeHtml(payment.payment_method)}</td>
          <td>${dateLabel(payment.payment_date)}</td>
          <td><span class="status-badge status-available">${escapeHtml(payment.payment_status)}</span></td>
        </tr>`;
      }).join("")
    : '<tr><td colspan="7" class="table-message">No payments are stored yet.</td></tr>';
  renderReservationChoices();
}

function renderWaitingList() {
  $("#waitingCount").textContent = state.waitingEntries.length;
  $("#waitingEntries").innerHTML = state.waitingEntries.length
    ? state.waitingEntries.map((entry, index) => `<div class="waiting-entry">
        <span class="waiting-position">${String(index + 1).padStart(2, "0")}</span>
        <span class="waiting-person">${escapeHtml(entry.customer_name)}</span>
        <span class="waiting-entry-meta">${index === 0 ? "Next to be notified" : `Queue position ${index + 1}`}</span>
      </div>`).join("")
    : '<div class="empty-queue">The waiting list is empty.</div>';
}

async function refreshData(showNotice = false) {
  const refreshButton = $("#refreshButton");
  refreshButton?.classList.add("is-loading");
  try {
    const [health, dashboard, customers, rooms, reservations, payments, waitingList] =
      await Promise.all([
        api("/api/health"),
        api("/api/dashboard"),
        api("/api/customers"),
        api("/api/rooms"),
        api("/api/reservations"),
        api("/api/payments"),
        api("/api/waiting-list")
      ]);
    updateConnectionStatus(health.status === "ok");
    state.dashboard = dashboard;
    state.customers = customers.items;
    state.rooms = rooms.items;
    state.reservations = reservations.items;
    state.payments = payments.items;
    state.waitingEntries = waitingList.items;
    renderDashboard();
    renderCustomers();
    renderRooms();
    renderReservations();
    renderPayments();
    renderWaitingList();
    if (showNotice) showToast("Dashboard data refreshed from the backend.");
  } catch (error) {
    updateConnectionStatus(false);
    showToast(error.message, true);
  } finally {
    refreshButton?.classList.remove("is-loading");
  }
}

function navigateTo(pageName) {
  const page = document.querySelector(`[data-page="${CSS.escape(pageName)}"]`);
  if (!page) return;
  $$(".page").forEach((section) => section.classList.toggle("active", section === page));
  $$(".nav-link").forEach((link) => {
    const isActive = link.dataset.pageLink === pageName;
    link.classList.toggle("active", isActive);
    if (isActive) link.setAttribute("aria-current", "page");
    else link.removeAttribute("aria-current");
  });
  const label = $(`.nav-link[data-page-link="${CSS.escape(pageName)}"] span`);
  $("#currentPageName").textContent = label?.textContent || "Overview";
  $("#sidebar").classList.remove("open");
  $("#menuToggle").setAttribute("aria-expanded", "false");
  window.scrollTo({ top: 0, behavior: "smooth" });
}

function formValues(form) {
  const values = Object.fromEntries(new FormData(form).entries());
  for (const element of Array.from(form.elements)) {
    if (element.name && element.type === "number" && element.value !== "") {
      values[element.name] = Number(element.value);
    }
  }
  return values;
}

function updateReservationEstimate() {
  const checkIn = $("#reservationCreateForm [name=check_in_date]").value;
  const checkOut = $("#reservationCreateForm [name=check_out_date]").value;
  const roomSelect = $("#reservationRoom");
  const option = roomSelect.selectedOptions[0];
  if (!checkIn || !checkOut) {
    $("#calculatedNights").textContent = "— nights";
    $("#calculatedTotal").textContent = "—";
    return;
  }
  const start = Date.parse(`${checkIn}T00:00:00Z`);
  const end = Date.parse(`${checkOut}T00:00:00Z`);
  const nights = Math.round((end - start) / 86400000);
  if (!Number.isFinite(nights) || nights <= 0) {
    $("#calculatedNights").textContent = "Check-out must follow check-in";
    $("#calculatedTotal").textContent = "—";
    return;
  }
  $("#calculatedNights").textContent = `${nights} ${nights === 1 ? "night" : "nights"}`;
  const rate = Number(option?.dataset.rate);
  $("#calculatedTotal").textContent = Number.isFinite(rate)
    ? money((Math.round(rate * 100) * nights) / 100)
    : "Select a room";
}

function setDefaultDates() {
  const today = new Date();
  $("#todayLabel").textContent = today.toLocaleDateString(undefined, {
    weekday: "long",
    month: "long",
    day: "numeric",
    year: "numeric"
  });
  const dateValue = [
    today.getFullYear(),
    String(today.getMonth() + 1).padStart(2, "0"),
    String(today.getDate()).padStart(2, "0")
  ].join("-");
  const paymentDate = $("#paymentCreateForm [name=payment_date]");
  paymentDate.value = dateValue;
  $("#reservationCreateForm [name=check_in_date]").min = dateValue;
  $("#reservationCreateForm [name=check_out_date]").min = dateValue;
  const smartCheckIn = $("#smartRoomFinderForm [name=check_in_date]");
  const smartCheckOut = $("#smartRoomFinderForm [name=check_out_date]");
  smartCheckIn.min = dateValue;
  smartCheckOut.min = dateValue;
  smartCheckIn.value = dateValue;
}

function editCustomer(customerId) {
  const customer = customerById(Number(customerId));
  if (!customer) return;
  const form = $("#customerEditForm");
  for (const [field, value] of Object.entries(customer)) {
    if (form.elements[field]) form.elements[field].value = value;
  }
  setFormMessage("customerEditMessage", "");
  $("#customerEditDialog").showModal();
}

async function submitJsonForm(form, path, method, messageId, successMessage) {
  setFormMessage(messageId, "Saving…");
  try {
    await api(path, { method, body: formValues(form) });
    form.reset();
    setFormMessage(messageId, successMessage);
    showToast(successMessage);
    await refreshData();
    return true;
  } catch (error) {
    setFormMessage(messageId, error.message, true);
    showToast(error.message, true);
    return false;
  }
}

document.addEventListener("click", async (event) => {
  const nav = event.target.closest("[data-page-link]");
  if (nav) {
    event.preventDefault();
    navigateTo(nav.dataset.pageLink);
    return;
  }
  const smartRoomButton = event.target.closest("[data-smart-room]");
  if (smartRoomButton) {
    const roomNumber = Number(smartRoomButton.dataset.smartRoom);
    const recommendation = state.smartRecommendations.find(
      (item) => Number(item.room_number) === roomNumber
    );
    if (!recommendation) {
      showToast("This room recommendation is no longer available. Search again.", true);
      return;
    }
    const checkIn =
      $("#smartRoomFinderForm [name=check_in_date]").value;
    const checkOut =
      $("#smartRoomFinderForm [name=check_out_date]").value;
    const roomSelect = $("#reservationRoom");
    const roomValue = String(roomNumber);
    const optionExists = Array.from(roomSelect.options).some(
      (option) => option.value === roomValue
    );
    if (!optionExists) {
      const option = document.createElement("option");
      option.value = roomValue;
      option.dataset.rate = String(recommendation.price_per_night);
      option.textContent =
        `Room ${roomNumber} · ${recommendation.category} · ` +
        `${money(recommendation.price_per_night)} · recommended for these dates`;
      roomSelect.append(option);
    }
    navigateTo("reservations");
    $("#reservationCreateForm [name=check_in_date]").value = checkIn;
    $("#reservationCreateForm [name=check_out_date]").value = checkOut;
    roomSelect.value = roomValue;
    updateReservationEstimate();
    showToast(`Room ${roomNumber} selected. Continue with the existing reservation form.`);
    return;
  }
  const editButton = event.target.closest("[data-edit-customer]");
  if (editButton) {
    editCustomer(editButton.dataset.editCustomer);
    return;
  }
  const deleteButton = event.target.closest("[data-delete-customer]");
  if (deleteButton) {
    const customerId = Number(deleteButton.dataset.deleteCustomer);
    if (!window.confirm(`Delete customer ${customerId}? Customers with reservations cannot be deleted.`)) {
      return;
    }
    try {
      await api(`/api/customers/${customerId}`, { method: "DELETE" });
      showToast("Customer deleted.");
      await refreshData();
    } catch (error) {
      showToast(error.message, true);
    }
    return;
  }
  const actionButton = event.target.closest("[data-reservation-action]");
  if (actionButton) {
    const reservationId = Number(actionButton.dataset.id);
    const action = actionButton.dataset.reservationAction;
    const prompts = {
      cancel: `Cancel reservation ${reservationId}?`,
      "check-in": `Check in reservation ${reservationId}?`,
      "check-out": `Check out reservation ${reservationId}?`
    };
    if (!window.confirm(prompts[action])) return;
    try {
      const result = await api(
        `/api/reservations/${reservationId}/${action}`,
        { method: "POST", body: {} }
      );
      if (action === "check-out" && result.notified_customer) {
        showToast(`Checked out. ${result.notified_customer} was notified.`);
      } else {
        showToast(`Reservation ${reservationId} ${action === "check-in" ? "checked in" : action === "check-out" ? "checked out" : "cancelled"}.`);
      }
      await refreshData();
    } catch (error) {
      showToast(error.message, true);
    }
    return;
  }
});

$("#menuToggle").addEventListener("click", () => {
  const isOpen = $("#sidebar").classList.toggle("open");
  $("#menuToggle").setAttribute("aria-expanded", String(isOpen));
});
$("#refreshButton").addEventListener("click", () => refreshData(true));
$("#customerFilter").addEventListener("input", renderCustomers);

$("#customerSearchForm").addEventListener("submit", async (event) => {
  event.preventDefault();
  const customerId = $("#customerSearchId").value;
  if (!customerId) return;
  const result = $("#customerSearchResult");
  result.classList.add("empty-state");
  result.textContent = "Searching…";
  try {
    const customer = await api(`/api/customers/${encodeURIComponent(customerId)}`);
    renderCustomerSearchResult(customer);
  } catch (error) {
    result.classList.add("empty-state");
    result.textContent = error.message;
  }
});

$("#customerCreateForm").addEventListener("submit", async (event) => {
  event.preventDefault();
  await submitJsonForm(
    event.currentTarget,
    "/api/customers",
    "POST",
    "customerCreateMessage",
    "Customer registered successfully."
  );
});

$("#customerEditForm").addEventListener("submit", async (event) => {
  event.preventDefault();
  const form = event.currentTarget;
  const customerId = Number(form.elements.customer_id.value);
  const values = formValues(form);
  delete values.customer_id;
  setFormMessage("customerEditMessage", "Saving…");
  try {
    await api(`/api/customers/${customerId}`, {
      method: "PUT",
      body: values
    });
    $("#customerEditDialog").close();
    showToast("Customer updated successfully.");
    await refreshData();
  } catch (error) {
    setFormMessage("customerEditMessage", error.message, true);
  }
});

$("#closeEditDialog").addEventListener("click", () =>
  $("#customerEditDialog").close()
);
$("#cancelEditDialog").addEventListener("click", () =>
  $("#customerEditDialog").close()
);

$("#roomCreateForm").addEventListener("submit", async (event) => {
  event.preventDefault();
  await submitJsonForm(
    event.currentTarget,
    "/api/rooms",
    "POST",
    "roomCreateMessage",
    "Room added successfully."
  );
});

$("#smartRoomFinderForm [name=check_in_date]").addEventListener("change", (event) => {
  $("#smartRoomFinderForm [name=check_out_date]").min = event.currentTarget.value;
});

$("#smartRoomFinderForm").addEventListener("submit", async (event) => {
  event.preventDefault();
  const values = formValues(event.currentTarget);
  values.number_of_guests = Number(values.number_of_guests);
  values.maximum_budget_per_night =
    Number(values.maximum_budget_per_night);
  setFormMessage("smartRoomFinderMessage", "Searching with Uniform Cost Search…");
  try {
    const result = await api("/api/smart-room-finder", {
      method: "POST",
      body: values
    });
    state.smartRecommendations = result.items;
    renderSmartRoomRecommendations();
    setFormMessage(
      "smartRoomFinderMessage",
      result.count
        ? `${result.count} suitable ${result.count === 1 ? "room" : "rooms"} found.`
        : "No suitable rooms were found."
    );
  } catch (error) {
    state.smartRecommendations = [];
    renderSmartRoomRecommendations();
    setFormMessage("smartRoomFinderMessage", error.message, true);
  }
});

$("#roomFilters").addEventListener("click", (event) => {
  const button = event.target.closest("[data-room-filter]");
  if (!button) return;
  state.roomFilter = button.dataset.roomFilter;
  $$("#roomFilters .filter-chip").forEach((chip) =>
    chip.classList.toggle("active", chip === button)
  );
  renderRooms();
});

$("#reservationFilters").addEventListener("click", (event) => {
  const button = event.target.closest("[data-reservation-filter]");
  if (!button) return;
  state.reservationFilter = button.dataset.reservationFilter;
  $$("#reservationFilters .filter-chip").forEach((chip) =>
    chip.classList.toggle("active", chip === button)
  );
  renderReservations();
});

$("#reservationRoom").addEventListener("change", updateReservationEstimate);
$("#reservationCreateForm [name=check_in_date]").addEventListener("change", () => {
  const checkIn = $("#reservationCreateForm [name=check_in_date]").value;
  $("#reservationCreateForm [name=check_out_date]").min = checkIn || "";
  updateReservationEstimate();
});
$("#reservationCreateForm [name=check_out_date]").addEventListener(
  "change", updateReservationEstimate
);

$("#reservationCreateForm").addEventListener("submit", async (event) => {
  event.preventDefault();
  const values = formValues(event.currentTarget);
  const room = state.rooms.find(
    (item) => String(item.room_number) === String(values.room_number)
  );
  const start = Date.parse(`${values.check_in_date}T00:00:00Z`);
  const end = Date.parse(`${values.check_out_date}T00:00:00Z`);
  const nights = Math.round((end - start) / 86400000);
  if (!room || !Number.isInteger(nights) || nights <= 0) {
    setFormMessage(
      "reservationCreateMessage",
      "Choose an available room and a valid date range.",
      true
    );
    return;
  }
  values.reservation_id = Number(values.reservation_id);
  values.customer_id = Number(values.customer_id);
  values.room_number = Number(values.room_number);
  values.number_of_nights = nights;
  values.total_cost = Number(
    ((Math.round(Number(room.price_per_night) * 100) * nights) / 100).toFixed(2)
  );
  const form = event.currentTarget;
  setFormMessage("reservationCreateMessage", "Saving…");
  try {
    await api("/api/reservations", { method: "POST", body: values });
    form.reset();
    updateReservationEstimate();
    setFormMessage(
      "reservationCreateMessage",
      "Reservation created successfully."
    );
    showToast("Reservation created successfully.");
    await refreshData();
  } catch (error) {
    setFormMessage("reservationCreateMessage", error.message, true);
    showToast(error.message, true);
  }
});

$("#paymentReservation").addEventListener("change", (event) => {
  const option = event.target.selectedOptions[0];
  const amount = $("#paymentCreateForm [name=amount]");
  amount.value = option?.dataset.total || "";
});

$("#paymentCreateForm").addEventListener("submit", async (event) => {
  event.preventDefault();
  const form = event.currentTarget;
  const values = formValues(form);
  const reservationId = Number(values.reservation_id);
  values.amount = Number(values.amount);
  values.payment_date = values.payment_date;
  delete values.reservation_id;
  setFormMessage("paymentCreateMessage", "Saving…");
  try {
    await api(`/api/reservations/${reservationId}/payments`, {
      method: "POST",
      body: values
    });
    form.reset();
    $("#paymentCreateForm [name=payment_date]").value =
      new Date().toISOString().slice(0, 10);
    setFormMessage("paymentCreateMessage", "Payment recorded successfully.");
    showToast("Payment recorded successfully.");
    await refreshData();
  } catch (error) {
    setFormMessage("paymentCreateMessage", error.message, true);
    showToast(error.message, true);
  }
});

$("#waitingCreateForm").addEventListener("submit", async (event) => {
  event.preventDefault();
  await submitJsonForm(
    event.currentTarget,
    "/api/waiting-list",
    "POST",
    "waitingCreateMessage",
    "Guest added to the waiting list."
  );
});

setDefaultDates();
refreshData();
