/**
 * Class to encapsulate all logic needed to load the file page table. 
 * This table shows which files are stored on the datalogger. 
 */
export class FileTable {
    /**
     * @param {object} options
     * @param {string} options.headRowId Id of the <tr> inside <thead> that receives the column headers.
     * @param {string} options.bodyId Id of the <tbody> that receives the data rows.
     * @param {string} [options.paginationId] Id of the container that receives the prev/next paging controls.
     * @param {number} [options.pageSize] Number of rows fetched/shown per page. Defaults to 20.
     * @param {Array<{key: string, label: string, render?: (entry: any) => string}>} options.columns
     *        Data columns to display, in order. `render` overrides the plain `entry[key]` lookup.
     * @param {(entry: any) => void} [options.onDownload] Called when a row's download button is clicked.
     * @param {(entry: any) => void} [options.onDelete] Called when a row's delete button is clicked.
     */
    constructor({ headRowId, bodyId, paginationId, pageSize = 20, columns, onDownload, onDelete } = {}) {
        this.headRowId = headRowId;
        this.bodyId = bodyId;
        this.columns = columns ?? [];
        this.onDownload = onDownload;
        this.onDelete = onDelete;
        this.pageSize = pageSize;

        this.fetchPage = null;
        this.pages = new Map();       // pageIndex -> entries[], filled in lazily as pages are visited
        this.currentPage = 0;
        this.knownLastPage = null;    // pageIndex of the last existing page, null while still unknown
        this.loadToken = 0;           // guards against out-of-order responses when paging quickly

        this.renderHead();

        if (paginationId) {
            this.paginationEl = document.getElementById(paginationId);
            this.buildPagination();
        }
    }

    /**
     * Setup for the page controls
     */
    buildPagination() {
        this.paginationEl.innerHTML = "";

        this.prevPageButton = document.createElement("button");
        this.prevPageButton.type = "button";
        this.prevPageButton.className = "pagination-arrow";
        this.prevPageButton.setAttribute("aria-label", "Vorherige Seite");
        this.prevPageButton.innerHTML = "&#8592;";
        this.prevPageButton.addEventListener("click", () => this.goToPage(this.currentPage - 1));

        this.pageInfo = document.createElement("span");
        this.pageInfo.className = "pagination-info";

        this.nextPageButton = document.createElement("button");
        this.nextPageButton.type = "button";
        this.nextPageButton.className = "pagination-arrow";
        this.nextPageButton.setAttribute("aria-label", "Nächste Seite");
        this.nextPageButton.innerHTML = "&#8594;";
        this.nextPageButton.addEventListener("click", () => this.goToPage(this.currentPage + 1));

        this.paginationEl.appendChild(this.prevPageButton);
        this.paginationEl.appendChild(this.pageInfo);
        this.paginationEl.appendChild(this.nextPageButton);
    }

    /**
     * Rebuild the header of the table
     * @param {Array<{key: string, label: string, render?: (entry: any) => string}>} columns
     */
    setColumns(columns) {
        this.columns = columns ?? [];
        this.renderHead();
    }

    /**
     * Points the table at a new data source and loads its first page. 
     * 
     * @param {(offset: number, limit: number) => Promise<{ok: boolean, status: number, data: {entries: any[]}}>} fetchPage
     *        Fetches one page of entries from the backend, e.g. `(offset, limit) => api.listConfigs(offset, limit)`.
     */
    async setSource(fetchPage) {
        this.fetchPage = fetchPage;
        this.pages = new Map();
        this.knownLastPage = null;
        this.currentPage = 0;
        this.loadToken++;   // invalidate any load still in flight from the previous source

        await this.loadPage(0);
    }

    /**
     * Navigates to the given page or fetches it from the backend first if it hasn't been loaded yet. 
     * @param {number} pageIndex index of the target page.
     */
    async goToPage(pageIndex) {
        if (pageIndex < 0 || pageIndex === this.currentPage) {
            return;
        }
        if (this.knownLastPage !== null && pageIndex > this.knownLastPage) {
            return;
        }
        await this.loadPage(pageIndex);
    }

    /**
     * Fetches the page with the passed index and renders it.
     * 
     * @param {number} pageIndex index of the page which should be loaded.
     */
    async loadPage(pageIndex) {
        if (!this.pages.has(pageIndex)) {
            const token = ++this.loadToken;
            this.renderLoading();

            const result = await this.fetchPage(pageIndex * this.pageSize, this.pageSize);
            if (token !== this.loadToken) {
                return;   // superseded by a newer setSource()/goToPage() call
            }

            if (!result.ok) {
                alert("Dateien konnten nicht geladen werden. Status: " + result.status);
                this.render();   // restore the previously shown page
                return;
            }

            const entries = result.data?.entries ?? [];
            this.pages.set(pageIndex, entries);
            if (entries.length < this.pageSize) {
                this.knownLastPage = pageIndex;
            }
        }

        this.currentPage = pageIndex;
        this.render();
    }

    renderHead() {
        const headRow = document.getElementById(this.headRowId);
        headRow.innerHTML = "";

        this.columns.forEach((column) => {
            const th = document.createElement("th");
            th.textContent = column.label;
            headRow.appendChild(th);
        });

        headRow.appendChild(document.createElement("th")); // download
        headRow.appendChild(document.createElement("th")); // delete
    }

    render() {
        const tbody = document.getElementById(this.bodyId);
        tbody.innerHTML = "";

        const entries = this.pages.get(this.currentPage) ?? [];
        if (entries.length === 0) {
            tbody.appendChild(this.createEmptyRow());
        } else {
            entries.forEach((entry) => tbody.appendChild(this.createRow(entry)));
        }

        this.updatePagination();
    }

    /** 
     * Shows a placeholder row while a not-yet-cached page is being fetched. 
     */
    renderLoading() {
        const tbody = document.getElementById(this.bodyId);
        tbody.innerHTML = "";

        const tr = document.createElement("tr");
        const td = document.createElement("td");
        td.className = "empty-row";
        td.colSpan = this.columns.length + 2;
        td.textContent = "Lädt…";
        tr.appendChild(td);
        tbody.appendChild(tr);
    }

    /**
     * Updates the page indicator text and disables the arrow buttons at the start/end of the available range.
     */
    updatePagination() {
        if (!this.paginationEl) {
            return;
        }

        this.pageInfo.textContent = this.knownLastPage !== null
            ? `Seite ${this.currentPage + 1} von ${this.knownLastPage + 1}`
            : `Seite ${this.currentPage + 1}`;

        this.prevPageButton.disabled = this.currentPage === 0;
        this.nextPageButton.disabled = this.knownLastPage !== null && this.currentPage >= this.knownLastPage;
    }

    createEmptyRow() {
        const tr = document.createElement("tr");
        const td = document.createElement("td");
        td.className = "empty-row";
        td.colSpan = this.columns.length + 2;
        td.textContent = "Keine Dateien vorhanden.";
        tr.appendChild(td);
        return tr;
    }

    createRow(entry) {
        const tr = document.createElement("tr");

        this.columns.forEach((column) => {
            const td = document.createElement("td");
            td.dataset.label = column.label;

            const value = document.createElement("span");
            value.className = "cell-value";
            value.textContent = column.render ? column.render(entry) : entry[column.key] ?? "";
            td.appendChild(value);

            tr.appendChild(td);
        });

        tr.appendChild(this.createActionCell("download-btn", "download-icon.svg", "⬇", "Herunterladen", () => this.onDownload?.(entry)));
        tr.appendChild(this.createActionCell("delete-btn", "delete-icon.svg", "🗑", "Löschen", () => this.onDelete?.(entry)));

        return tr;
    }

    /**
     * Creates a cell for the download and delete button.
     * 
     * @param {string} label Text shown before the icon on mobile screens; hidden on desktop.
     */
    createActionCell(btnClass, icon, alt, label, onClick) {
        const td = document.createElement("td");
        td.className = "actions-cell";

        const button = document.createElement("button");
        button.className = `table-btn ${btnClass}`;
        button.innerHTML = `<span class="table-btn-label">${label}</span><img src="${icon}" alt="${alt}" class="table-icon">`;
        button.addEventListener("click", onClick);

        td.appendChild(button);
        return td;
    }
}
