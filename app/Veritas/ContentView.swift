import SwiftUI

struct ContentView: View {
    var body: some View {
        VStack(spacing: 16) {
            Text("🦉")
                .font(.system(size: 80))
            Text("Hello, Veritas!")
                .font(.largeTitle)
                .bold()
            Text("Pill screener: coming soon")
                .foregroundStyle(.secondary)
        }
        .padding()
    }
}

#Preview {
    ContentView()
}
