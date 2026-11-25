import SwiftUI

// these are the owl's moods. these match the eys on the devices OLED screen
enum OwlMood: CaseIterable {
    case sleepy // no device connected
    case awake // connected
    case lookingDown // shake / scanning
    
}
