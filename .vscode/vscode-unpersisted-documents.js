exports.GET = function(args) {
    var vscode = require("vscode");

    function describeDocument(document) {
        return {
            uri: document.uri.toString(),
            fsPath: document.uri.fsPath || null,
            fileName: document.fileName,
            languageId: document.languageId,
            isUntitled: document.isUntitled,
            isDirty: document.isDirty,
            version: document.version,
            lineCount: document.lineCount
        };
    }

    var activeDocument = vscode.window.activeTextEditor
        ? vscode.window.activeTextEditor.document
        : null;

    args.setContent(JSON.stringify({
        ok: true,
        activeDocument: activeDocument ? describeDocument(activeDocument) : null,
        unpersistedDocuments: vscode.workspace.textDocuments
            .filter(function(document) {
                return document.isDirty;
            })
            .map(describeDocument)
    }), "application/json");
};
